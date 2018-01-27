#include "ScoreProcessor.h"
#include "ExecutionManager.h"
#include "ScenePlayer.h"

using namespace std;

// ScoreProcessor-s -------------------------------------------

vector<shared_ptr<SusDrawableNoteData>> ScoreProcessor::DefaultDataValue;

AutoPlayerProcessor::AutoPlayerProcessor(ScenePlayer *player)
{
    Player = player;
}

void AutoPlayerProcessor::Reset()
{
    Player->CurrentResult->Reset();
    data = Player->data;
    int an = 0;
    for (auto &note : data) {
        auto type = note->Type.to_ulong();
        if (type & SU_NOTE_LONG_MASK) {
            if (!note->Type.test((size_t)SusNoteType::AirAction)) an++;
            for (auto &ex : note->ExtraData)
                if (
                    ex->Type.test((size_t)SusNoteType::End)
                    || ex->Type.test((size_t)SusNoteType::Step)
                    || ex->Type.test((size_t)SusNoteType::Injection))
                    an++;
        } else if (type & SU_NOTE_SHORT_MASK) {
            an++;
        }
    }
    Player->CurrentResult->SetAllNotes(an);
}

bool AutoPlayerProcessor::ShouldJudge(std::shared_ptr<SusDrawableNoteData> note)
{
    double current = Player->CurrentTime - note->StartTime + Player->SoundBufferingLatency;
    double extra = 0.5;
    if (note->Type.to_ulong() & SU_NOTE_LONG_MASK) {
        return current >= -extra && current - note->Duration <= extra;
    } else if (note->Type.to_ulong() & SU_NOTE_SHORT_MASK) {
        return current >= -extra && current <= extra;
    }
    return false;
}

void AutoPlayerProcessor::Update(vector<shared_ptr<SusDrawableNoteData>> &notes)
{
    bool SlideCheck = false;
    bool HoldCheck = false;
    bool AACheck = false;
    for (auto& note : notes) {
        ProcessScore(note);
        SlideCheck = isInSlide || SlideCheck;
        HoldCheck = isInHold || HoldCheck;
        AACheck = isInAA || AACheck;
    }

    if (!wasInSlide && SlideCheck) Player->PlaySoundSlide();
    if (wasInSlide && !SlideCheck) Player->StopSoundSlide();
    if (!wasInHold && HoldCheck) Player->PlaySoundHold();
    if (wasInHold && !HoldCheck) Player->StopSoundHold();
    Player->AirActionShown = AACheck;

    wasInHold = HoldCheck;
    wasInSlide = SlideCheck;
    wasInAA = AACheck;
}

void AutoPlayerProcessor::MovePosition(double relative)
{
    double newTime = Player->CurrentTime + relative;
    Player->CurrentResult->Reset();

    wasInHold = isInHold = false;
    wasInSlide = isInSlide = false;
    Player->StopSoundHold();
    Player->StopSoundSlide();
    Player->RemoveSlideEffect();

    // 送り: 飛ばした部分をFinishedに
    // 戻し: 入ってくる部分をUn-Finishedに
    for (auto &note : data) {
        if (note->Type.test((size_t)SusNoteType::Hold)
            || note->Type.test((size_t)SusNoteType::Slide)
            || note->Type.test((size_t)SusNoteType::AirAction)) {
            if (note->StartTime <= newTime) {
                note->OnTheFlyData.set((size_t)NoteAttribute::Finished);
            } else {
                note->OnTheFlyData.reset((size_t)NoteAttribute::Finished);
            }
            for (auto &extra : note->ExtraData) {
                if (!extra->Type.test((size_t)SusNoteType::End)
                    && !extra->Type.test((size_t)SusNoteType::Step)
                    && !extra->Type.test((size_t)SusNoteType::Injection)) continue;
                if (extra->StartTime <= newTime) {
                    extra->OnTheFlyData.set((size_t)NoteAttribute::Finished);
                } else {
                    extra->OnTheFlyData.reset((size_t)NoteAttribute::Finished);
                }
            }
        } else {
            if (note->StartTime <= newTime) {
                note->OnTheFlyData.set((size_t)NoteAttribute::Finished);
            } else {
                note->OnTheFlyData.reset((size_t)NoteAttribute::Finished);
            }
        }
    }
}

void AutoPlayerProcessor::Draw()
{}

void AutoPlayerProcessor::ProcessScore(shared_ptr<SusDrawableNoteData> note)
{
    double relpos = Player->CurrentTime - note->StartTime + Player->SoundBufferingLatency;
    if (relpos < 0 || (note->OnTheFlyData.test((size_t)NoteAttribute::Finished) && note->ExtraData.size() == 0)) return;

    if (note->Type.test((size_t)SusNoteType::Hold)) {
        isInHold = true;
        if (!note->OnTheFlyData.test((size_t)NoteAttribute::Finished)) {
            Player->PlaySoundTap();
            Player->SpawnJudgeEffect(note, JudgeType::ShortNormal);
            IncrementCombo(AbilityNoteType::Hold);
            note->OnTheFlyData.set((size_t)NoteAttribute::Finished);
        }

        for (auto &extra : note->ExtraData) {
            double pos = Player->CurrentTime - extra->StartTime + Player->SoundBufferingLatency;
            if (pos < 0) continue;
            if (extra->Type.test((size_t)SusNoteType::End)) isInHold = false;
            if (extra->OnTheFlyData.test((size_t)NoteAttribute::Finished)) continue;
            if (extra->Type[(size_t)SusNoteType::Injection]) {
                IncrementCombo(AbilityNoteType::Hold);
                extra->OnTheFlyData.set((size_t)NoteAttribute::Finished);
                return;
            }
            Player->PlaySoundTap();
            Player->SpawnJudgeEffect(note, JudgeType::ShortNormal);
            IncrementCombo(AbilityNoteType::Hold);
            extra->OnTheFlyData.set((size_t)NoteAttribute::Finished);
            return;
        }
    } else if (note->Type.test((size_t)SusNoteType::Slide)) {
        isInSlide = true;
        if (!note->OnTheFlyData.test((size_t)NoteAttribute::Finished)) {
            Player->PlaySoundTap();
            Player->SpawnSlideLoopEffect(note);

            IncrementCombo(AbilityNoteType::Slide);
            note->OnTheFlyData.set((size_t)NoteAttribute::Finished);
            return;
        }
        for (auto &extra : note->ExtraData) {
            double pos = Player->CurrentTime - extra->StartTime + Player->SoundBufferingLatency;
            if (pos < 0) continue;
            if (extra->Type.test((size_t)SusNoteType::End)) isInSlide = false;
            if (extra->Type.test((size_t)SusNoteType::Control)) continue;
            if (extra->Type.test((size_t)SusNoteType::Invisible)) continue;
            if (extra->OnTheFlyData.test((size_t)NoteAttribute::Finished)) continue;
            if (extra->Type.test((size_t)SusNoteType::Injection)) {
                IncrementCombo(AbilityNoteType::Slide);
                extra->OnTheFlyData.set((size_t)NoteAttribute::Finished);
                return;
            }
            Player->PlaySoundTap();
            Player->SpawnJudgeEffect(extra, JudgeType::SlideTap);
            IncrementCombo(AbilityNoteType::Slide);
            extra->OnTheFlyData.set((size_t)NoteAttribute::Finished);
            return;
        }
    } else if (note->Type.test((size_t)SusNoteType::AirAction)) {
        isInAA = true;
        for (auto &extra : note->ExtraData) {
            double pos = Player->CurrentTime - extra->StartTime + Player->SoundBufferingLatency;
            if (pos < 0) continue;
            if (extra->Type.test((size_t)SusNoteType::End)) isInAA = false;
            if (extra->Type.test((size_t)SusNoteType::Control)) continue;
            if (extra->Type.test((size_t)SusNoteType::Invisible)) continue;
            if (extra->OnTheFlyData.test((size_t)NoteAttribute::Finished)) continue;
            if (extra->Type[(size_t)SusNoteType::Injection]) {
                IncrementCombo(AbilityNoteType::AirAction);
                extra->OnTheFlyData.set((size_t)NoteAttribute::Finished);
                return;
            }
            Player->PlaySoundAirAction();
            Player->SpawnJudgeEffect(extra, JudgeType::Action);
            IncrementCombo(AbilityNoteType::AirAction);
            extra->OnTheFlyData.set((size_t)NoteAttribute::Finished);
        }
    } else if (note->Type.test((size_t)SusNoteType::Air)) {
        Player->PlaySoundAir();
        Player->SpawnJudgeEffect(note, JudgeType::ShortNormal);
        Player->SpawnJudgeEffect(note, JudgeType::ShortEx);
        IncrementCombo(AbilityNoteType::Air);
        note->OnTheFlyData.set((size_t)NoteAttribute::Finished);
    } else if (note->Type.test((size_t)SusNoteType::Tap)) {
        Player->PlaySoundTap();
        Player->SpawnJudgeEffect(note, JudgeType::ShortNormal);
        IncrementCombo(AbilityNoteType::Tap);
        note->OnTheFlyData.set((size_t)NoteAttribute::Finished);
    } else if (note->Type.test((size_t)SusNoteType::ExTap)) {
        Player->PlaySoundExTap();
        Player->SpawnJudgeEffect(note, JudgeType::ShortNormal);
        Player->SpawnJudgeEffect(note, JudgeType::ShortEx);
        IncrementCombo(AbilityNoteType::ExTap);
        note->OnTheFlyData.set((size_t)NoteAttribute::Finished);
    } else if (note->Type.test((size_t)SusNoteType::Flick)) {
        Player->PlaySoundFlick();
        Player->SpawnJudgeEffect(note, JudgeType::ShortNormal);
        IncrementCombo(AbilityNoteType::Flick);
        note->OnTheFlyData.set((size_t)NoteAttribute::Finished);
    } else if (note->Type.test((size_t)SusNoteType::HellTap)) {
        Player->PlaySoundTap();
        Player->SpawnJudgeEffect(note, JudgeType::ShortNormal);
        IncrementCombo(AbilityNoteType::HellTap);
        note->OnTheFlyData.set((size_t)NoteAttribute::Finished);
    }
}

void AutoPlayerProcessor::IncrementCombo(AbilityNoteType type)
{
    Player->CurrentResult->PerformJusticeCritical();
    Player->CurrentCharacterInstance->OnJusticeCritical(type);
}
