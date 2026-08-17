// Copyright (c) 2026, kiwec, All rights reserved.
#include "StoryboardTest.h"

#include "TestMacros.h"
#include "Engine.h"
#include "Environment.h"
#include "File.h"
#include "Paths.h"
#include "Storyboard.h"

#include <string>
#include <string_view>

namespace Mc::Tests {

namespace {
void writeFile(const std::string &path, std::string_view bytes) {
    File file(path, File::MODE::WRITE);
    (void)file.write({reinterpret_cast<const u8 *>(bytes.data()), bytes.size()});
}

Storyboard storyboard(std::string_view events, std::string_view osb = {}) {
    const std::string osu = fmt::format("osu file format v14\n\n[Events]\n{}\n", events);
    return Storyboard{"", osu, osb, ""};
}
}  // namespace

StoryboardTest::StoryboardTest() { logRaw("StoryboardTest created"); }

void StoryboardTest::update() {
    if(!m_ran) {
        m_ran = true;
        runTests();

        TEST_PRINT_RESULTS("StoryboardTest");

        engine->shutdown();
    }
}

void StoryboardTest::runTests() {
    using Layer = Storyboard::Layer;

    TEST_SECTION("commands");
    {
        const auto sb = storyboard(
            "Sprite,Foreground,Centre,\"sb\\a.png\",320,240\n"
            " F,0,1000,2000,0,1\n"
            " M,0,1000,3000,0,0,100,100\n");
        TEST_ASSERT_EQ(sb.getNumSprites(), 1u, "one sprite");
        TEST_ASSERT(!sb.getSpriteState(0, 500), "not there before it fades in");
        const auto mid = sb.getSpriteState(0, 1500);
        TEST_ASSERT(mid.has_value(), "there while fading in");
        if(mid) {
            TEST_ASSERT_NEAR(mid->alpha, 0.5f, 1e-4f, "halfway faded in");
            TEST_ASSERT_NEAR(mid->pos.x, 25.f, 1e-3f, "moved a quarter of the way");
        }
        const auto held = sb.getSpriteState(0, 2500);
        TEST_ASSERT(held && held->alpha == 1.f, "keeps the end value after a command");
        TEST_ASSERT(!sb.getSpriteState(0, 3000), "gone once the last command ends");
        TEST_ASSERT_EQ(sb.getEndTime().value_or(0.0), 3000.0, "end time");
    }

    TEST_SECTION("easing");
    {
        const auto sb = storyboard("Sprite,Background,TopLeft,a.png,0,0\n_F,1,0,1000,0,1\n_S,0,0,1000,2\n");
        const auto state = sb.getSpriteState(0, 500);
        TEST_ASSERT(state.has_value(), "sprite there");
        if(state) {
            TEST_ASSERT_NEAR(state->alpha, 0.75f, 1e-4f, "easing 1 decelerates (osu!'s \"Out\")");
            TEST_ASSERT_NEAR(state->scale.x, 2.f, 1e-4f, "a single value holds over the whole command");
        }
    }

    TEST_SECTION("loops");
    {
        const auto sb = storyboard(
            "Sprite,Pass,Centre,a.png,320,240\n"
            " L,1000,3\n"
            "  F,0,0,500,0,1\n"
            "  F,0,500,1000,1,0\n");
        TEST_ASSERT(!sb.getSpriteState(0, 900), "not there before the loop");
        const auto first = sb.getSpriteState(0, 1250);
        const auto second = sb.getSpriteState(0, 2250);
        const auto fadingOut = sb.getSpriteState(0, 2750);
        TEST_ASSERT(first && second && fadingOut, "there during the loop");
        if(first && second && fadingOut) {
            TEST_ASSERT_NEAR(first->alpha, 0.5f, 1e-4f, "first iteration");
            TEST_ASSERT_NEAR(second->alpha, 0.5f, 1e-4f, "second iteration");
            TEST_ASSERT_NEAR(fadingOut->alpha, 0.5f, 1e-4f, "second iteration fading out");
        }
        TEST_ASSERT(sb.getSpriteState(0, 3999).has_value(), "there until the last iteration ends");
        TEST_ASSERT(!sb.getSpriteState(0, 4000), "gone after the last iteration");
    }

    TEST_SECTION("shorthand");
    {
        const auto sb = storyboard("Sprite,Background,Centre,a.png,0,0\n_M,0,0,100,0,0,10,10,20,0\n");
        const auto state = sb.getSpriteState(0, 150);
        TEST_ASSERT(state.has_value(), "sprite there");
        if(state) {
            TEST_ASSERT_NEAR(state->pos.x, 15.f, 1e-4f, "second step x");
            TEST_ASSERT_NEAR(state->pos.y, 5.f, 1e-4f, "second step y");
        }
    }

    TEST_SECTION("parameters");
    {
        const auto sb = storyboard(
            "Sprite,Background,Centre,a.png,0,0\n"
            "_F,0,0,1000,1\n"
            "_P,0,100,200,H\n"
            "_P,0,300,,V\n");
        const auto before = sb.getSpriteState(0, 50);
        const auto during = sb.getSpriteState(0, 150);
        const auto after = sb.getSpriteState(0, 250);
        TEST_ASSERT(before && during && after, "sprite there");
        if(before && during && after) {
            TEST_ASSERT(!before->flipH && during->flipH && !after->flipH, "a lasting parameter only while it lasts");
            TEST_ASSERT(before->flipV && after->flipV, "an instant parameter for good (from the start, as in osu!)");
        }
    }

    TEST_SECTION("quirks");
    {
        const auto sb = storyboard(
            "Sprite,Background,Centre,a.png,0,0\n"
            "_F,0,0,1000,1.5\n"
            "Sprite,Background,Centre,b.png,0,0\n"
            "Sprite,3,1,c.png,1,2\n"
            "_F,0,500,,0.25\n"
            "_F,0,500,1000,0.25\n");
        TEST_ASSERT_EQ(sb.getNumSprites(), 2u, "a sprite without commands is left out");
        const auto flicker = sb.getSpriteState(0, 500);
        TEST_ASSERT(flicker && std::abs(flicker->alpha - 0.5f) < 1e-4f, "an opacity above 1 wraps around");
        const auto numeric = sb.getSpriteState(1, 600);
        TEST_ASSERT(numeric && numeric->pos == vec2(1.f, 2.f) && numeric->alpha == 0.25f,
                    "layer and origin by number, an empty end time");
        TEST_ASSERT(!storyboard("Sprite,Nowhere,Centre,a.png,0,0\n_F,0,0,1,1\n").getNumSprites(), "unknown layer");
    }

    TEST_SECTION("variables");
    {
        const auto sb = storyboard(
            "", "[Variables]\n$x=320\n$pic=\"a b.png\"\n\n[Events]\nSprite,Overlay,Centre,$pic,$x,240\n_F,0,0,10,1\n");
        const auto state = sb.getSpriteState(0, 5);
        TEST_ASSERT(state && state->pos == vec2(320.f, 240.f), "variables replaced in the .osb");
    }

    TEST_SECTION("triggers");
    {
        auto sb = storyboard(
            "Sprite,Foreground,Centre,a.png,0,0\n"
            "_F,0,0,10000,1\n"
            "_T,Passing,0,10000\n"
            "__F,0,0,100,0,1\n"
            "_T,HitSoundDrumWhistle\n"
            "__S,0,0,100,3\n");
        sb.update(500, false, false);
        TEST_ASSERT(sb.getSpriteState(0, 550)->alpha == 1.f, "nothing fired yet");
        sb.update(600, true, false);
        TEST_ASSERT_NEAR(sb.getSpriteState(0, 650)->alpha, 0.5f, 1e-4f, "Passing fired on the change to passing");
        sb.onHitSound({.normalSet = 0, .additionSet = 1, .additions = 1, .index = 0}, 700);
        TEST_ASSERT(sb.getSpriteState(0, 750)->scale.x == 1.f, "a whistle of another set doesn't fire it");
        sb.onHitSound({.normalSet = 0, .additionSet = 2, .additions = 1, .index = 0}, 800);
        TEST_ASSERT(sb.getSpriteState(0, 850)->scale.x == 3.f, "a drum whistle fires it");
        sb.update(500, true, true);
        TEST_ASSERT(sb.getSpriteState(0, 550)->alpha == 1.f, "seeking forgets what fired");
    }

    // (from osu!lazer's test resources)
    TEST_SECTION("lifetimes");
    {
        const auto loops = storyboard(
            "Sprite,Background,TopCentre,\"zero-times.png\",320,240\n"
            " L,1000,0\n"
            "  F,0,0,1000,0,1\n"
            "  F,0,1000,2000,1,0\n"
            "Sprite,Background,TopCentre,\"many-times.png\",320,240\n"
            " L,9000,40\n"
            "  F,0,0,1000,0,1\n"
            "  F,0,1000,2000,1,0\n");
        TEST_ASSERT(loops.getSpriteState(0, 2999) && !loops.getSpriteState(0, 3000), "a loop runs at least once");
        TEST_ASSERT(loops.getSpriteState(1, 88999) && !loops.getSpriteState(1, 89000), "a loop runs its count");

        const auto noop = storyboard(
            "Sprite,Background,TopCentre,\"img.jpg\",320,240\n"
            " F,0,1000,1000,0,0 // should be ignored\n"
            " F,0,1500,1600,0,1\n");
        TEST_ASSERT(!noop.getSpriteState(0, 1499) && noop.getSpriteState(0, 1500), "invisible fades don't count");

        const auto loopFade = storyboard(
            "Sprite,Background,TopCentre,\"img.jpg\",320,240\n"
            " L,1000,1\n"
            "  F,0,0,,1\n"
            " F,0,2000,,0\n");
        TEST_ASSERT(loopFade.getSpriteState(0, 1000).has_value(), "a visible fade in a loop counts");

        const auto animation = storyboard(
            "Animation,Background,Centre,\"img.jpg\",320,240,2,150,LoopForever\n"
            " S,0,1000,1500,0.08 // animation should start playing from this point in time..\n"
            " F,0,2000,,0,1      // .. not this point in time\n"
            " F,0,2000,3000,1\n"
            "Animation,Foreground,Centre,\"once.png\",330,240,10,108,1\n"
            " F,0,0,10000,1\n");
        TEST_ASSERT(!animation.getSpriteState(0, 1999), "an animation shows once it fades in");
        const auto frame = animation.getSpriteState(0, 2150);
        TEST_ASSERT(frame && frame->frame == 1, "and its frames count from its first command");
        const auto once = animation.getSpriteState(1, 5000);
        TEST_ASSERT(once && once->frame == 9, "an animation played once stays at its last frame");

        const auto suffix = storyboard(
            "", "[Variables]\n$var=34\n\n[Events]\nSprite,Background,TopCentre,\"img.jpg\",$var56,240\n_F,0,0,1,1\n");
        TEST_ASSERT(suffix.getSpriteState(0, 0) && suffix.getSpriteState(0, 0)->pos.x == 3456.f,
                    "variable with a suffix");
    }

    TEST_SECTION("hash");
    {
        const std::string dir = Mc::Paths::cache() + "/storyboardtest/";
        Environment::createDirectory(dir + "SB");
        writeFile(dir + "SB/a.png", "a");
        writeFile(dir + "b.png", "b");
        writeFile(dir + "bg.jpg", "bg");
        const std::string ab{
            "osu file format v14\n\n[Events]\nSprite,Background,Centre,\"sb\\A.png\",0,0\n_F,0,0,1,1\n"
            "Sprite,Overlay,Centre,b.png,0,0\n_F,0,0,1,1\n"};
        const std::string ba{
            "osu file format v14\n\n[Events]\nSprite,Overlay,Centre,b.png,0,0\n_F,0,0,1,1\n"
            "Sprite,Background,Centre,\"sb\\A.png\",0,0\n_F,0,0,1,1\n"};
        const auto hashOf = [&dir](std::string_view osu, std::string_view osb = {}) {
            Storyboard sb{dir, osu, osb, "bg.jpg"};
            sb.load("");
            return sb.getHash();
        };

        const MD5Hash first = hashOf(ab);
        TEST_ASSERT(first == hashOf(ab), "the same storyboard, the same hash");
        TEST_ASSERT(first == hashOf(ba), "whatever order the script uses its files in");
        TEST_ASSERT(first != hashOf(ab, "[Events]\n"), "an .osb file counts");
        writeFile(dir + "SB/a.png", "changed");
        TEST_ASSERT(first != hashOf(ab), "a changed image counts");
        writeFile(dir + "SB/a.png", "a");
        writeFile(dir + "bg.jpg", "changed");
        TEST_ASSERT(first != hashOf(ab), "a changed background counts");
        Environment::deletePathsRecursive(dir, 3);
    }

    TEST_SECTION("background");
    {
        const std::string osu{
            "osu file format "
            "v14\n\n[Events]\n0,0,\"bg.jpg\",10,-20\nSprite,Background,Centre,\"BG.jpg\",320,240\n_F,0,0,10,1\n"};
        const Storyboard sb{"", osu, "", "bg.jpg"};
        TEST_ASSERT(sb.replacesBackground(), "a background layer sprite of the background replaces it");
        TEST_ASSERT(sb.getBackgroundOffset() == vec2(10.f, -20.f), "background offset");
        TEST_ASSERT(sb.getSpriteState(0, 5).has_value(), "sprite there");
        (void)Layer::OVERLAY;
    }
}

}  // namespace Mc::Tests
