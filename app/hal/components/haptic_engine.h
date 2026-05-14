/**
 * @file haptic_engine.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-11
 *
 * @copyright Copyright (c) 2024
 *
 */
// https://www.ti.com/cn/lit/ds/symlink/drv2605l.pdf
#pragma once
#include <vector>

namespace HapticEffect {
enum HapticEffect_t {
    StrongClick100 = 1,                     // Strong click - 100%
    StrongClick60,                          // Strong click - 60%
    StrongClick30,                          // Strong click - 30%
    SharpClick100,                          // Sharp click - 100%
    SharpClick60,                           // Sharp click - 60%
    SharpClick30,                           // Sharp click - 30%
    SoftBump100,                            // Soft bump - 100%
    SoftBump60,                             // Soft bump - 60%
    SoftBump30,                             // Soft bump - 30%
    DoubleClick100,                         // Double click - 100%
    DoubleClick60,                          // Double click - 60%
    TripleClick100,                         // Triple click - 100%
    SoftFuzz60,                             // Soft fuzz - 60%
    StrongBuzz100,                          // Strong buzz - 100%
    Alert750ms100,                          // 750ms alert - 100%
    Alert1000ms100,                         // 1000ms alert - 100%
    StrongClick1_100,                       // Strong click 1 - 100%
    StrongClick2_80,                        // Strong click 2 - 80%
    StrongClick3_60,                        // Strong click 3 - 60%
    StrongClick4_30,                        // Strong click 4 - 30%
    MediumClick1_100,                       // Medium click 1 - 100%
    MediumClick2_80,                        // Medium click 2 - 80%
    MediumClick3_60,                        // Medium click 3 - 60%
    SharpTick1_100,                         // Sharp tick 1 - 100%
    SharpTick2_80,                          // Sharp tick 2 - 80%
    SharpTick3_60,                          // Sharp tick 3 - 60%
    ShortDoubleClickStrong1_100,            // Short double click strong 1 - 100%
    ShortDoubleClickStrong2_80,             // Short double click strong 2 - 80%
    ShortDoubleClickStrong3_60,             // Short double click strong 3 - 60%
    ShortDoubleClickStrong4_30,             // Short double click strong 4 - 30%
    ShortDoubleClickMedium1_100,            // Short double click medium 1 - 100%
    ShortDoubleClickMedium2_80,             // Short double click medium 2 - 80%
    ShortDoubleClickMedium3_60,             // Short double click medium 3 - 60%
    ShortDoubleSharpTick1_100,              // Short double sharp tick 1 - 100%
    ShortDoubleSharpTick2_80,               // Short double sharp tick 2 - 80%
    ShortDoubleSharpTick3_60,               // Short double sharp tick 3 - 60%
    LongDoubleSharpClickStrong1_100,        // Long double sharp click strong 1 - 100%
    LongDoubleSharpClickStrong2_80,         // Long double sharp click strong 2 - 80%
    LongDoubleSharpClickStrong3_60,         // Long double sharp click strong 3 - 60%
    LongDoubleSharpClickStrong4_30,         // Long double sharp click strong 4 - 30%
    LongDoubleSharpClickMedium1_100,        // Long double sharp click medium 1 - 100%
    LongDoubleSharpClickMedium2_80,         // Long double sharp click medium 2 - 80%
    LongDoubleSharpClickMedium3_60,         // Long double sharp click medium 3 - 60%
    LongDoubleSharpTick1_100,               // Long double sharp tick 1 - 100%
    LongDoubleSharpTick2_80,                // Long double sharp tick 2 - 80%
    LongDoubleSharpTick3_60,                // Long double sharp tick 3 - 60%
    Buzz1_100,                              // Buzz 1 - 100%
    Buzz2_80,                               // Buzz 2 - 80%
    Buzz3_60,                               // Buzz 3 - 60%
    Buzz4_40,                               // Buzz 4 - 40%
    Buzz5_20,                               // Buzz 5 - 20%
    PulsingStrong1_100,                     // Pulsing strong 1 - 100%
    PulsingStrong2_60,                      // Pulsing strong 2 - 60%
    PulsingMedium1_100,                     // Pulsing medium 1 - 100%
    PulsingMedium2_60,                      // Pulsing medium 2 - 60%
    PulsingSharp1_100,                      // Pulsing sharp 1 - 100%
    PulsingSharp2_60,                       // Pulsing sharp 2 - 60%
    TransitionClick1_100,                   // Transition click 1 - 100%
    TransitionClick2_80,                    // Transition click 2 - 80%
    TransitionClick3_60,                    // Transition click 3 - 60%
    TransitionClick4_40,                    // Transition click 4 - 40%
    TransitionClick5_20,                    // Transition click 5 - 20%
    TransitionClick6_10,                    // Transition click 6 - 10%
    TransitionHum1_100,                     // Transition hum 1 - 100%
    TransitionHum2_80,                      // Transition hum 2 - 80%
    TransitionHum3_60,                      // Transition hum 3 - 60%
    TransitionHum4_40,                      // Transition hum 4 - 40%
    TransitionHum5_20,                      // Transition hum 5 - 20%
    TransitionHum6_10,                      // Transition hum 6 - 10%
    TransitionRampDownLongSmooth1_100To0,   // Long smooth ramp-down transition 1 - 100 to 0%
    TransitionRampDownLongSmooth2_100To0,   // Long smooth ramp-down transition 2 - 100 to 0%
    TransitionRampDownMediumSmooth1_100To0, // Medium smooth ramp-down transition 1 - 100 to 0%
    TransitionRampDownMediumSmooth2_100To0, // Medium smooth ramp-down transition 2 - 100 to 0%
    TransitionRampDownShortSmooth1_100To0,  // Short smooth ramp-down transition 1 - 100 to 0%
    TransitionRampDownShortSmooth2_100To0,  // Short smooth ramp-down transition 2 - 100 to 0%
    TransitionRampDownLongSharp1_100To0,    // Long sharp ramp-down transition 1 - 100 to 0%
    TransitionRampDownLongSharp2_100To0,    // Long sharp ramp-down transition 2 - 100 to 0%
    TransitionRampDownMediumSharp1_100To0,  // Medium sharp ramp-down transition 1 - 100 to 0%
    TransitionRampDownMediumSharp2_100To0,  // Medium sharp ramp-down transition 2 - 100 to 0%
    TransitionRampDownShortSharp1_100To0,   // Short sharp ramp-down transition 1 - 100 to 0%
    TransitionRampDownShortSharp2_100To0,   // Short sharp ramp-down transition 2 - 100 to 0%
    TransitionRampUpLongSmooth1_0To100,     // Long smooth ramp-up transition 1 - 0 to 100%
    TransitionRampUpLongSmooth2_0To100,     // Long smooth ramp-up transition 2 - 0 to 100%
    TransitionRampUpMediumSmooth1_0To100,   // Medium smooth ramp-up transition 1 - 0 to 100%
    TransitionRampUpMediumSmooth2_0To100,   // Medium smooth ramp-up transition 2 - 0 to 100%
    TransitionRampUpShortSmooth1_0To100,    // Short smooth ramp-up transition 1 - 0 to 100%
    TransitionRampUpShortSmooth2_0To100,    // Short smooth ramp-up transition 2 - 0 to 100%
    TransitionRampUpLongSharp1_0To100,      // Long sharp ramp-up transition 1 - 0 to 100%
    TransitionRampUpLongSharp2_0To100,      // Long sharp ramp-up transition 2 - 0 to 100%
    TransitionRampUpMediumSharp1_0To100,    // Medium sharp ramp-up transition 1 - 0 to 100%
    TransitionRampUpMediumSharp2_0To100,    // Medium sharp ramp-up transition 2 - 0 to 100%
    TransitionRampUpShortSharp1_0To100,     // Short sharp ramp-up transition 1 - 0 to 100%
    TransitionRampUpShortSharp2_0To100,     // Short sharp ramp-up transition 2 - 0 to 100%
    TransitionRampDownLongSmooth1_50To0,    // Long smooth ramp-down transition 1 - 50 to 0%
    TransitionRampDownLongSmooth2_50To0,    // Long smooth ramp-down transition 2 - 50 to 0%
    TransitionRampDownMediumSmooth1_50To0,  // Medium smooth ramp-down transition 1 - 50 to 0%
    TransitionRampDownMediumSmooth2_50To0,  // Medium smooth ramp-down transition 2 - 50 to 0%
    TransitionRampDownShortSmooth1_50To0,   // Short smooth ramp-down transition 1 - 50 to 0%
    TransitionRampDownShortSmooth2_50To0,   // Short smooth ramp-down transition 2 - 50 to 0%
    TransitionRampDownLongSharp1_50To0,     // Long sharp ramp-down transition 1 - 50 to 0%
    TransitionRampDownLongSharp2_50To0,     // Long sharp ramp-down transition 2 - 50 to 0%
    TransitionRampDownMediumSharp1_50To0,   // Medium sharp ramp-down transition 1 - 50 to 0%
    TransitionRampDownMediumSharp2_50To0,   // Medium sharp ramp-down transition 2 - 50 to 0%
    TransitionRampDownShortSharp1_50To0,    // Short sharp ramp-down transition 1 - 50 to 0%
    TransitionRampDownShortSharp2_50To0,    // Short sharp ramp-down transition 2 - 50 to 0%
    TransitionRampUpLongSmooth1_0To50,      // Long smooth ramp-up transition 1 - 0 to 50%
    TransitionRampUpLongSmooth2_0To50,      // Long smooth ramp-up transition 2 - 0 to 50%
    TransitionRampUpMediumSmooth1_0To50,    // Medium smooth ramp-up transition 1 - 0 to 50%
    TransitionRampUpMediumSmooth2_0To50,    // Medium smooth ramp-up transition 2 - 0 to 50%
    TransitionRampUpShortSmooth1_0To50,     // Short smooth ramp-up transition 1 - 0 to 50%
    TransitionRampUpShortSmooth2_0To50,     // Short smooth ramp-up transition 2 - 0 to 50%
    TransitionRampUpLongSharp1_0To50,       // Long sharp ramp-up transition 1 - 0 to 50%
    TransitionRampUpLongSharp2_0To50,       // Long sharp ramp-up transition 2 - 0 to 50%
    TransitionRampUpMediumSharp1_0To50,     // Medium sharp ramp-up transition 1 - 0 to 50%
    TransitionRampUpMediumSharp2_0To50,     // Medium sharp ramp-up transition 2 - 0 to 50%
    TransitionRampUpShortSharp1_0To50,      // Short sharp ramp-up transition 1 - 0 to 50%
    TransitionRampUpShortSharp2_0To50,      // Short sharp ramp-up transition 2 - 0 to 50%
    LongBuzzForProgrammaticStopping100,     // Long buzz for programmatic stopping - 100%
    SmoothHum1_NoKickOrBrakePulse_50,       // Smooth hum 1 (no kick or brake pulse) - 50%
    SmoothHum2_NoKickOrBrakePulse_40,       // Smooth hum 2 (no kick or brake pulse) - 40%
    SmoothHum3_NoKickOrBrakePulse_30,       // Smooth hum 3 (no kick or brake pulse) - 30%
    SmoothHum4_NoKickOrBrakePulse_20,       // Smooth hum 4 (no kick or brake pulse) - 20%
    SmoothHum5_NoKickOrBrakePulse_10        // Smooth hum 5 (no kick or brake pulse) - 10%
};
}

namespace hal_components {

/**
 * @brief Haptic feedback (linear motor) component base class
 *
 */
class HapticEngineBase {
public:
    virtual bool init()
    {
        return false;
    }

    virtual void enable() {}

    virtual void disable() {}

    /**
     * @brief Play a haptic effect
     *
     * @param effect
     */
    virtual void playEffect(const HapticEffect::HapticEffect_t& effect) {}

    /**
     * @brief Play a sequence of haptic effects
     *
     * @param effectSequence effect sequence, up to 8 entries
     */
    virtual void playEffects(const std::vector<HapticEffect::HapticEffect_t>& effectSequence) {}

    virtual void stop() {}

    virtual bool isPlaying()
    {
        return false;
    }
};

} // namespace hal_components
