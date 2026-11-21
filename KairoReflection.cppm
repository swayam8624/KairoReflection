// SOURCE STUDY NOTE:
// Umbrella reexport for metadata types and registry, allowing KairoEditor and serialization code to
// consume reflection without depending on a window or ECS implementation.
// Cross-file guide: docs/SOURCE_READING_ORDER.md.

export module Kairo.Reflection;

export import Kairo.Reflection.Types;
export import Kairo.Reflection.Registry;

