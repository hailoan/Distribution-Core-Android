### File GLSL

- base func: [base_fragment_shade_top.glsl](../../DistributationLibrary/videogl/src/main/assets/glsl/base_fragment_shade_top.glsl)
- adjust func: [frag_base_shader_adjust_v2.glsl](../../DistributationLibrary/videogl/src/main/assets/glsl/frag_base_shader_adjust_v2.glsl)
- mode filter: [frag_base_shader_blend_v2.glsl](../../DistributationLibrary/videogl/src/main/assets/glsl/frag_base_shader_blend_v2.glsl)

### My requirement

Only update code into module `:videolib` first
- implement adjust + filter on preview video
- can build and pass a new filter from top-level module
- code isn't depend on anything (flexible)
