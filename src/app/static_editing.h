// UI-only exclusions. Keep upstream commands/data structures for scripts and file IO.
#pragma once
#include <string>

namespace app {
constexpr bool kStaticEditing = true;
inline bool isAnimationCommand(const std::string& id)
{
  static const char* commands[] = {
    "FrameProperties", "FrameTagProperties", "GotoFirstFrame", "GotoFirstFrameInTag",
    "GotoPreviousFrame", "GotoPreviousFrameWithSameTag", "GotoNextFrame", "GotoNextFrameWithSameTag",
    "GotoLastFrame", "GotoLastFrameInTag", "GotoFrame", "NewFrame", "NewFrameTag",
    "RemoveFrame", "RemoveFrameTag", "ReverseFrames", "PlayAnimation", "PlayPreviewAnimation",
    "ShowOnionSkin", "SetPlaybackSpeed", "SetLoopSection", "TogglePlayAll", "TogglePlayOnce",
    "TogglePlaySubtags", "ToggleRewindOnStop", "ToggleTimelineThumbnails", "LinkCels", "UnlinkCel",
    "CopyCel", "ClearCel", "CelProperties", "CelOpacity", "MoveCel", "ImportSpriteSheet", "ExportSpriteSheet"
  };
  for (const auto* command : commands) if (id == command) return true;
  return false;
}
} // namespace app
