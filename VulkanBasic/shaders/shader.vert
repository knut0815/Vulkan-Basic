#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject
{
  mat4 model;
  mat4 view;
  mat4 projection;
} ubo;

out gl_PerVertex
{
  vec4 gl_Position;
};

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vTexCoord;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

// note: the GL_ARB_separate_shader_objects extension is required for Vulkan shaders

void main()
{
  vColor = inColor;
  vTexCoord = inTexCoord;
  gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
}
