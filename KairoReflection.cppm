// TEACHING MODULE: C++23 umbrella that re-exports only metadata value and registry
// interfaces. EngineCore/Editor can import Kairo.Reflection without adopting an
// ECS, ImGui or graphics-device dependency; implementations live in the two
// imported module files.

export module Kairo.Reflection;

export import Kairo.Reflection.Types;
export import Kairo.Reflection.Registry;

