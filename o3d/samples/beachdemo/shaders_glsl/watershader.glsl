// glslv profile log:
// (104) : warning C7011: implicit cast from "float4" to "float3"
// (107) : warning C7011: implicit cast from "float4" to "float3"
// (110) : warning C7011: implicit cast from "float4" to "float3"
// (126) : warning C7011: implicit cast from "float4" to "float3"
// (137) : warning C7011: implicit cast from "float4" to "float3"
// 157 lines, 5 warnings, 0 errors.

// glslf profile log:
// (104) : warning C7011: implicit cast from "float4" to "float3"
// (107) : warning C7011: implicit cast from "float4" to "float3"
// (110) : warning C7011: implicit cast from "float4" to "float3"
// (126) : warning C7011: implicit cast from "float4" to "float3"
// (137) : warning C7011: implicit cast from "float4" to "float3"
// 157 lines, 5 warnings, 0 errors.

// glslv output by Cg compiler
// cgc version 2.0.0010, build date Dec 12 2007
// command line args: -profile glslv
//vendor NVIDIA Corporation
//version 2.0.0.10
//profile glslv
//program vertexShaderFunction
//semantic world : WORLD
//semantic worldInverseTranspose : WORLDINVERSETRANSPOSE
//semantic view : VIEW
//semantic viewProjection : VIEWPROJECTION
//semantic worldViewProjection : WORLDVIEWPROJECTION
//semantic viewPosition
//semantic waterColor
//semantic reflectionRefractionOffset
//semantic clock
//semantic environmentSampler
//semantic fresnelSampler
//semantic refractionSampler
//semantic reflectionSampler
//semantic noiseSampler
//semantic noiseSampler2
//semantic noiseSampler3
//var float4x4 world : WORLD : _ZZ2Sworld[0], 4 : -1 : 1
//var float4x4 worldInverseTranspose : WORLDINVERSETRANSPOSE : , 4 : -1 : 0
//var float4x4 view : VIEW : , 4 : -1 : 0
//var float4x4 viewProjection : VIEWPROJECTION : _ZZ2SviewProjection[0], 4 : -1 : 1
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : , 4 : -1 : 0
//var float3 viewPosition :  : _ZZ2SviewPosition : -1 : 1
//var float4 waterColor :  :  : -1 : 0
//var float reflectionRefractionOffset :  : _ZZ2SreflectionRefractionOffset : -1 : 1
//var float clock :  :  : -1 : 0
//var samplerCUBE environmentSampler :  :  : -1 : 0
//var sampler2D fresnelSampler :  :  : -1 : 0
//var sampler2D refractionSampler :  :  : -1 : 0
//var sampler2D reflectionSampler :  :  : -1 : 0
//var sampler2D noiseSampler :  :  : -1 : 0
//var sampler2D noiseSampler2 :  :  : -1 : 0
//var sampler2D noiseSampler3 :  :  : -1 : 0
//var float4 input.position : $vin.POSITION : POSITION : 0 : 1
//var float2 input.texcoord : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float4 vertexShaderFunction.position : $vout.POSITION : POSITION : -1 : 1
//var float2 vertexShaderFunction.texcoord : $vout.TEXCOORD0 : TEXCOORD0 : -1 : 1
//var float3 vertexShaderFunction.viewVector : $vout.TEXCOORD1 : TEXCOORD1 : -1 : 1
//var float3 vertexShaderFunction.screenPosition : $vout.TEXCOORD2 : TEXCOORD2 : -1 : 1

attribute vec4 position;
attribute vec4 texcoord0;
vec4 _glPositionTemp;
uniform vec4 dx_clipping;

struct VertexShaderInput {
    vec4 position;
    vec2 texcoord;
};

struct PixelShaderInput {
    vec4 position;
    vec2 texcoord;
    vec3 viewVector;
    vec3 screenPosition;
};

PixelShaderInput _ZZ3Sret_0;
vec4 _ZZ3SrZh0017;
vec4 _ZZ3SrZh0019;
vec3 _ZZ3SZDtemp20;
vec3 _ZZ3SvZh0021;
float _ZZ3SxZh0025;
vec4 _ZZ3SrZh0027;
vec4 _ZZ3SvZh0027;
uniform mat4 world;
uniform mat4 viewprojection;
uniform vec3 viewPosition;
uniform float reflectionRefractionOffset;

 // main procedure, the original name was vertexShaderFunction
void main()
{

    PixelShaderInput _ZZ4Soutput;

    _ZZ3SrZh0017 = position.x*world[0];
    _ZZ3SrZh0017 = _ZZ3SrZh0017 + position.y*world[1];
    _ZZ3SrZh0017 = _ZZ3SrZh0017 + position.z*world[2];
    _ZZ3SrZh0017 = _ZZ3SrZh0017 + position.w*world[3];
    _ZZ3SrZh0019 = _ZZ3SrZh0017.x*viewprojection[0];
    _ZZ3SrZh0019 = _ZZ3SrZh0019 + _ZZ3SrZh0017.y*viewprojection[1];
    _ZZ3SrZh0019 = _ZZ3SrZh0019 + _ZZ3SrZh0017.z*viewprojection[2];
    _ZZ3SrZh0019 = _ZZ3SrZh0019 + _ZZ3SrZh0017.w*viewprojection[3];
    _ZZ3SvZh0021 = _ZZ3SrZh0017.xyz - viewPosition.xyz;
    _ZZ3SxZh0025 = dot(_ZZ3SvZh0021, _ZZ3SvZh0021);
    _ZZ3SZDtemp20 = inversesqrt(_ZZ3SxZh0025)*_ZZ3SvZh0021;
    _ZZ3SvZh0027 = vec4(_ZZ3SrZh0017.x, _ZZ3SrZh0017.y, 0.00000000E+00, 1.00000000E+00);
    _ZZ3SrZh0027 = _ZZ3SvZh0027.x*viewprojection[0];
    _ZZ3SrZh0027 = _ZZ3SrZh0027 + _ZZ3SvZh0027.y*viewprojection[1];
    _ZZ3SrZh0027 = _ZZ3SrZh0027 + _ZZ3SvZh0027.z*viewprojection[2];
    _ZZ3SrZh0027 = _ZZ3SrZh0027 + _ZZ3SvZh0027.w*viewprojection[3];
    _ZZ4Soutput.screenPosition = _ZZ3SrZh0027.xyz/_ZZ3SrZh0027.w;
    _ZZ4Soutput.screenPosition.xy = 5.00000000E-01 + (5.00000000E-01*_ZZ4Soutput.screenPosition.xy)*vec2( 1.00000000E+00, -1.00000000E+00);
    _ZZ4Soutput.screenPosition.z = reflectionRefractionOffset/_ZZ4Soutput.screenPosition.z;
    _ZZ3Sret_0.position = _ZZ3SrZh0019;
    _ZZ3Sret_0.texcoord = texcoord0.xy;
    _ZZ3Sret_0.viewVector = _ZZ3SZDtemp20;
    _ZZ3Sret_0.screenPosition = _ZZ4Soutput.screenPosition;
    gl_TexCoord[0].xy = texcoord0.xy;
    gl_TexCoord[2].xyz = _ZZ4Soutput.screenPosition;
    _glPositionTemp = _ZZ3SrZh0019; gl_Position = vec4(_glPositionTemp.x + _glPositionTemp.w * dx_clipping.x, dx_clipping.w * (_glPositionTemp.y + _glPositionTemp.w * dx_clipping.y), _glPositionTemp.z * 2.0 - _glPositionTemp.w, _glPositionTemp.w);
    gl_TexCoord[1].xyz = _ZZ3SZDtemp20;
    return;
} // main end


// #o3d SplitMarker
// #o3d MatrixLoadOrder RowMajor

// glslf output by Cg compiler
// cgc version 2.0.0010, build date Dec 12 2007
// command line args: -profile glslf
//vendor NVIDIA Corporation
//version 2.0.0.10
//profile glslf
//program pixelShaderFunction
//semantic world : WORLD
//semantic worldInverseTranspose : WORLDINVERSETRANSPOSE
//semantic view : VIEW
//semantic viewProjection : VIEWPROJECTION
//semantic worldViewProjection : WORLDVIEWPROJECTION
//semantic viewPosition
//semantic waterColor
//semantic reflectionRefractionOffset
//semantic clock
//semantic environmentSampler
//semantic fresnelSampler
//semantic refractionSampler
//semantic reflectionSampler
//semantic noiseSampler
//semantic noiseSampler2
//semantic noiseSampler3
//var float4x4 world : WORLD : , 4 : -1 : 0
//var float4x4 worldInverseTranspose : WORLDINVERSETRANSPOSE : , 4 : -1 : 0
//var float4x4 view : VIEW : , 4 : -1 : 0
//var float4x4 viewProjection : VIEWPROJECTION : , 4 : -1 : 0
//var float4x4 worldViewProjection : WORLDVIEWPROJECTION : , 4 : -1 : 0
//var float3 viewPosition :  :  : -1 : 0
//var float4 waterColor :  :  : -1 : 0
//var float reflectionRefractionOffset :  :  : -1 : 0
//var float clock :  : _ZZ2Sclock : -1 : 1
//var samplerCUBE environmentSampler :  : _ZZ2SenvironmentSampler : -1 : 1
//var sampler2D fresnelSampler :  : _ZZ2SfresnelSampler : -1 : 1
//var sampler2D refractionSampler :  : _ZZ2SrefractionSampler : -1 : 1
//var sampler2D reflectionSampler :  : _ZZ2SreflectionSampler : -1 : 1
//var sampler2D noiseSampler :  : _ZZ2SnoiseSampler : -1 : 1
//var sampler2D noiseSampler2 :  : _ZZ2SnoiseSampler2 : -1 : 1
//var sampler2D noiseSampler3 :  : _ZZ2SnoiseSampler3 : -1 : 1
//var float2 input.texcoord : $vin.TEXCOORD0 : TEXCOORD0 : 0 : 1
//var float3 input.viewVector : $vin.TEXCOORD1 : TEXCOORD1 : 0 : 1
//var float3 input.screenPosition : $vin.TEXCOORD2 : TEXCOORD2 : 0 : 1
//var float4 pixelShaderFunction : $vout.COLOR : COLOR : -1 : 1



struct VertexShaderInput {
    vec2 texcoord;
};

struct PixelShaderInput {
    vec2 texcoord;
    vec3 viewVector;
    vec3 screenPosition;
};

vec4 _ZZ3Sret_0;
vec2 _ZZ3ScZh0019;
vec2 _ZZ3ScZh0021;
vec2 _ZZ3ScZh0023;
vec3 _ZZ3SZDtemp24;
vec3 _ZZ3SvZh0025;
float _ZZ3SxZh0029;
vec3 _ZZ3SZDtemp30;
vec3 _ZZ3SZDtemp32;
float _ZZ3SxZh0039;
vec2 _ZZ3ScZh0043;
vec2 _ZZ3ScZh0045;
vec2 _ZZ3ScZh0051;
uniform float clock;
uniform samplerCube environmentSampler;
uniform sampler2D fresnelSampler;
uniform sampler2D refractionSampler;
uniform sampler2D reflectionSampler;
uniform sampler2D noiseSampler;
uniform sampler2D noiseSampler2;
uniform sampler2D noiseSampler3;

 // main procedure, the original name was pixelShaderFunction
void main()
{

    vec3 _ZZ4SviewVector;
    vec2 _ZZ4Stexcoord;
    vec3 _ZZ4Sn1;
    vec3 _ZZ4Sn2;
    vec3 _ZZ4Sn3;
    vec3 _ZZ4SR;
    float _ZZ4Sf;
    vec4 _ZZ4Sreflection;
    vec3 _ZZ4SskyReflection;
    vec3 _ZZ4Scolor;
    vec3 _ZZ4Srefraction;
    vec3 _ZZ4SfinalColor;

    _ZZ4SviewVector = vec3(gl_TexCoord[1].x, gl_TexCoord[1].z, -gl_TexCoord[1].y);
    _ZZ4Stexcoord = gl_TexCoord[0].xy*4.00000000E+00;
    _ZZ3ScZh0019 = _ZZ4Stexcoord + vec2(clock*9.99999978E-03, clock*1.99999996E-02);
    _ZZ4Sn1 = texture2D(noiseSampler, _ZZ3ScZh0019).xyz;
    _ZZ3ScZh0021 = _ZZ4Stexcoord + vec2(clock*2.99999993E-02, clock*9.99999978E-03);
    _ZZ4Sn2 = texture2D(noiseSampler2, _ZZ3ScZh0021).xyz;
    _ZZ3ScZh0023 = _ZZ4Stexcoord + vec2(clock*4.99999989E-03, clock*7.00000022E-03);
    _ZZ4Sn3 = texture2D(noiseSampler3, _ZZ3ScZh0023).xyz;
    _ZZ3SvZh0025 = _ZZ4Sn1 + _ZZ4Sn2*2.00000000E+00 + _ZZ4Sn3*4.00000000E+00 + vec3( -3.50000000E+00, 1.60000000E+01, -3.50000000E+00);
    _ZZ3SxZh0029 = dot(_ZZ3SvZh0025, _ZZ3SvZh0025);
    _ZZ3SZDtemp24 = inversesqrt(_ZZ3SxZh0029)*_ZZ3SvZh0025;
    _ZZ3SZDtemp30 = _ZZ4SviewVector - (2.00000000E+00*_ZZ3SZDtemp24)*dot(_ZZ3SZDtemp24, _ZZ4SviewVector);
    _ZZ3SxZh0039 = dot(_ZZ3SZDtemp30, _ZZ3SZDtemp30);
    _ZZ3SZDtemp32 = inversesqrt(_ZZ3SxZh0039)*_ZZ3SZDtemp30;
    _ZZ4SR = _ZZ3SZDtemp32;
    _ZZ4SR.y = _ZZ3SZDtemp32.y < 9.99999978E-03 ? 9.99999978E-03 : _ZZ3SZDtemp32.y;
    _ZZ3ScZh0043 = vec2(dot(_ZZ4SR, _ZZ3SZDtemp24), 5.00000000E-01);
    _ZZ4Sf = texture2D(fresnelSampler, _ZZ3ScZh0043).x;
    _ZZ3ScZh0045 = (gl_TexCoord[2].xy - gl_TexCoord[2].z*_ZZ3SZDtemp24.xy) + vec2( 0.00000000E+00, 1.00000001E-01);
    _ZZ4Sreflection = texture2D(reflectionSampler, _ZZ3ScZh0045);
    _ZZ4SskyReflection = textureCube(environmentSampler, _ZZ4SR).xyz;
    _ZZ4Scolor = _ZZ4SskyReflection + _ZZ4Sreflection.w*(_ZZ4Sreflection.xyz - _ZZ4SskyReflection);
    _ZZ3ScZh0051 = ((gl_TexCoord[2].xy - gl_TexCoord[2].z*_ZZ3SZDtemp24.xz) + vec2( 0.00000000E+00, 5.00000007E-02))*vec2( 1.00000000E+00, 9.49999988E-01);
    _ZZ4Srefraction = texture2D(refractionSampler, _ZZ3ScZh0051).xyz;
    _ZZ4SfinalColor = _ZZ4Srefraction + _ZZ4Sf*(_ZZ4Scolor - _ZZ4Srefraction);
    _ZZ3Sret_0 = vec4(_ZZ4SfinalColor.x, _ZZ4SfinalColor.y, _ZZ4SfinalColor.z, 1.00000000E+00);
    gl_FragColor = _ZZ3Sret_0;
    return;
} // main end

