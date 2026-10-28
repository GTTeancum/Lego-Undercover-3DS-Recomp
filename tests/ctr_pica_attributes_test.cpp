#include "services/pica_startup.h"
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <vector>

using namespace lego::ctr;
namespace {
int failures{};
#define CHECK(x) do {if (!(x)) {std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures;}} while (0)

struct List {
    std::vector<std::uint8_t> bytes;
    void Word(std::uint32_t value) {
        for (unsigned i=0; i<4; ++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));
    }
    void Packet(unsigned id, std::uint32_t value, unsigned mask=15,
                std::initializer_list<std::uint32_t> extra={}, bool grouped=false) {
        Word(value);
        Word(id | (mask<<16) | (static_cast<std::uint32_t>(extra.size())<<20) |
             (grouped ? 0x80000000U : 0));
        for (auto word:extra) Word(word);
        if (extra.size()&1U) Word(0xDEADC0DE);
    }
};
struct Fixture {
    PicaGpuRegisters regs{};
    PicaUploadState uploads{};
    PicaListPlan plan{};
    bool Run(const List& list) {return StagePicaStartupList(list.bytes,regs,uploads,plan);}
    void Commit() {regs=plan.registers; uploads=plan.uploads;}
    void Stop(const List& list) {
        const auto before_regs=regs;
        const auto before_uploads=uploads;
        CHECK(!Run(list)); CHECK(plan.result.error);
        CHECK(regs==before_regs && uploads==before_uploads);
    }
};

// Independent literal vectors exercise all word boundaries in the documented
// four-float24/three-word transfer. Values are XYZW, not submission word order.
constexpr std::array<std::uint32_t,4> raw{0x123456,0x789ABC,0xDEF012,0x345678};
constexpr std::array<std::uint32_t,4> ieee{0x291A2B00,0x5C4D5E00,0xCF780900,0x3A2B3C00};
constexpr std::uint32_t a=0x345678DE, b=0xF012789A, c=0xBC123456;

void PackedValuesAndFifoPorts() {
    Fixture f;
    CHECK(!f.uploads.default_attributes.index_known);
    CHECK(f.uploads.default_attributes.written.none());
    List grouped;
    grouped.Packet(0x232,2,15,{a,b,c},true); // Index plus all three data ports.
    CHECK(f.Run(grouped));
    CHECK(f.plan.uploads.default_attributes.raw24[2]==raw);
    CHECK(f.plan.uploads.default_attributes.floats[2]==ieee);
    CHECK(f.plan.uploads.default_attributes.written.count()==1);
    CHECK(f.plan.uploads.default_attributes.index_known);
    CHECK(f.plan.uploads.default_attributes.packed_count==0);
    CHECK(f.plan.registers[0x632]==3 && f.plan.result.attribute_vectors==1);
    CHECK(f.plan.result.irqs==0 && f.plan.uploads.gs==f.uploads.gs && f.plan.uploads.vs==f.uploads.vs);

    List repeated;
    repeated.Packet(0x232,4);
    repeated.Packet(0x235,a,15,{b,c}); // One port is also a FIFO, not a component selector.
    CHECK(f.Run(repeated));
    CHECK(f.plan.uploads.default_attributes.raw24[4]==raw && f.plan.registers[0x635]==c);

    List shuffled;
    shuffled.Packet(0x232,6); shuffled.Packet(0x235,a);
    shuffled.Packet(0x233,b); shuffled.Packet(0x234,c);
    CHECK(f.Run(shuffled)); CHECK(f.plan.uploads.default_attributes.raw24[6]==raw);

    List special;
    special.Packet(0x232,0); special.Packet(0x233,0x0000017F,15,{0x00008000,0x003F0000});
    CHECK(f.Run(special));
    // x=1, y=-0, z=+infinity, w=minimum float24 subnormal (lossless host conversion).
    CHECK((f.plan.uploads.default_attributes.raw24[0]==
           std::array<std::uint32_t,4>{0x3F0000,0x800000,0x7F0000,1}));
    CHECK((f.plan.uploads.default_attributes.floats[0]==
           std::array<std::uint32_t,4>{0x3F800000,0x80000000,0x7F800000,0x18800000}));
    CHECK(!f.plan.uploads.default_attributes.written[1]);
}

void ContinuationAndIndexReset() {
    Fixture f; List first;
    first.Packet(0x232,2); first.Packet(0x233,a);
    CHECK(f.Run(first));
    CHECK(f.plan.uploads.default_attributes.packed_count==1);
    CHECK(f.plan.uploads.default_attributes.written.none() && f.plan.result.attribute_vectors==0);
    f.Commit();
    List second; second.Packet(0x234,b); second.Packet(0x235,c);
    CHECK(f.Run(second)); CHECK(f.plan.uploads.default_attributes.raw24[2]==raw);
    CHECK(f.plan.registers[0x632]==3 && f.plan.result.attribute_vectors==1);
    Fixture together; first.Packet(0x234,b); first.Packet(0x235,c);
    CHECK(together.Run(first));
    CHECK(f.plan.uploads==together.plan.uploads && f.plan.registers==together.plan.registers);

    // Even an unchanged index write resets the partial FIFO. Once established,
    // its byte-masked register value determines the next destination.
    List reset; reset.Packet(0x232,7); reset.Packet(0x233,0xDEADBEEF);
    reset.Packet(0x232,0xFFFFFFFF,0); reset.Packet(0x235,a,15,{b,c});
    reset.Packet(0x233,0xBAD0BAD0); reset.Packet(0x232,9,1);
    reset.Packet(0x233,a,15,{b,c});
    CHECK(f.Run(reset));
    CHECK(f.plan.uploads.default_attributes.raw24[7]==raw);
    CHECK(f.plan.uploads.default_attributes.raw24[9]==raw);
    CHECK(!f.plan.uploads.default_attributes.written[8]);
    CHECK(f.plan.uploads.default_attributes.packed_count==0 && f.plan.registers[0x632]==10);
    CHECK(f.plan.result.attribute_vectors==2);
}

void BoundsUnknownStateAndRejection() {
    Fixture f; List unknown; unknown.Packet(0x233,a); f.Stop(unknown);
    for (unsigned mask:{0U,1U}) {
        List partial_index; partial_index.Packet(0x232,2,mask); f.Stop(partial_index);
    }

    List last; last.Packet(0x232,kPicaDefaultAttributeCount-1);
    last.Packet(0x233,a,15,{b,c});
    CHECK(f.Run(last));
    CHECK(f.plan.uploads.default_attributes.raw24[14]==raw && f.plan.registers[0x632]==15);
    f.Commit();
    List immediate; immediate.Packet(0x233,a); f.Stop(immediate);
    CHECK(!f.uploads.default_attributes.written[0]); // No cursor wrap into slot zero.
    for (std::uint32_t index:{15U,16U,0x10000002U}) {
        List bad; bad.Packet(0x232,index); bad.Packet(0x234,a); f.Stop(bad);
        CHECK(f.plan.result.register_id==0x234);
    }
    for (unsigned mask:{0U,5U}) {
        List partial_data; partial_data.Packet(0x232,2); partial_data.Packet(0x233,a,mask);
        f.Stop(partial_data); CHECK(f.plan.result.register_id==0x233);
    }

    // Supported attributes cannot hide a later unsupported draw or malformed
    // packet. Only the disposable plan may contain the successful prefix.
    List draw; draw.Packet(0x232,2); draw.Packet(0x233,a,15,{b,c}); draw.Packet(0x22E,1);
    f.Stop(draw); CHECK(f.plan.result.register_id==0x22E);
    List malformed; malformed.Packet(0x232,2); malformed.Packet(0x233,a,15,{b,c});
    malformed.Word(0); malformed.Word(0x003F0233);
    f.Stop(malformed);
}
} // namespace

int main() {
    PackedValuesAndFifoPorts(); ContinuationAndIndexReset(); BoundsUnknownStateAndRejection();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: PICA fixed attributes, packed values, FIFO continuation/reset, known state and retained draw guards\n";
    return EXIT_SUCCESS;
}
