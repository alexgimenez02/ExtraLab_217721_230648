//By Arnau Colom, Juan S. Marquerie
varying vec3 v_position;
//Optional to use
uniform vec4 u_color;
uniform float u_time;
uniform vec2 u_texture_resolution;

uniform mat4 u_inverse_viewprojection;
uniform mat4 u_viewprojection;

uniform vec2 u_iRes;
uniform vec3 u_camera_pos;
uniform sampler2D u_texture;

#define TAU 6.28318530718
#define MOD2 vec2(.16632,.17369)
#define MOD3 vec3(.16532,.17369,.15787)
float gTime;
float flareUp;

float random (in vec2 st) {
    return fract(sin(dot(st.xy,
                         vec2(12.9898,78.233)))
                 * 43758.5453123);
}
float dot2( in vec2 v ) { return dot(v,v); }
float dot2( in vec3 v ) { return dot(v,v); }
float ndot( in vec2 a, in vec2 b ) { return a.x*b.x - a.y*b.y; }
//-------------------------PERLIN NOISE-----------------
//  Simplex 4D Noise 
//  by Ian McEwan, Ashima Arts
//
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
float permute(float x){return floor(mod(((x*34.0)+1.0)*x, 289.0));}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
float taylorInvSqrt(float r){return 1.79284291400159 - 0.85373472095314 * r;}
vec2 Rotate2axis(vec2 p, float a)
{
	float si = sin(a);
	float co = cos(a);
	return mat2(si, co, -co, si) * p;
}

//=================================================================================================
// Linear step, faster than smoothstep...
float LinearStep(float a, float b, float x)
{
	return clamp((x-a)/(b-a), 0.0, 1.0);
}
vec4 grad4(float j, vec4 ip){
  const vec4 ones = vec4(1.0, 1.0, 1.0, -1.0);
  vec4 p,s;

  p.xyz = floor( fract (vec3(j) * ip.xyz) * 7.0) * ip.z - 1.0;
  p.w = 1.5 - dot(abs(p.xyz), ones.xyz);
  s = vec4(lessThan(p, vec4(0.0)));
  p.xyz = p.xyz + (s.xyz*2.0 - 1.0) * s.www; 

  return p;
}

float snoise(vec4 v){
  const vec2  C = vec2( 0.138196601125010504,  // (5 - sqrt(5))/20  G4
                        0.309016994374947451); // (sqrt(5) - 1)/4   F4
// First corner
  vec4 i  = floor(v + dot(v, C.yyyy) );
  vec4 x0 = v -   i + dot(i, C.xxxx);

// Other corners

// Rank sorting originally contributed by Bill Licea-Kane, AMD (formerly ATI)
  vec4 i0;

  vec3 isX = step( x0.yzw, x0.xxx );
  vec3 isYZ = step( x0.zww, x0.yyz );
//  i0.x = dot( isX, vec3( 1.0 ) );
  i0.x = isX.x + isX.y + isX.z;
  i0.yzw = 1.0 - isX;

//  i0.y += dot( isYZ.xy, vec2( 1.0 ) );
  i0.y += isYZ.x + isYZ.y;
  i0.zw += 1.0 - isYZ.xy;

  i0.z += isYZ.z;
  i0.w += 1.0 - isYZ.z;

  // i0 now contains the unique values 0,1,2,3 in each channel
  vec4 i3 = clamp( i0, 0.0, 1.0 );
  vec4 i2 = clamp( i0-1.0, 0.0, 1.0 );
  vec4 i1 = clamp( i0-2.0, 0.0, 1.0 );

  //  x0 = x0 - 0.0 + 0.0 * C 
  vec4 x1 = x0 - i1 + 1.0 * C.xxxx;
  vec4 x2 = x0 - i2 + 2.0 * C.xxxx;
  vec4 x3 = x0 - i3 + 3.0 * C.xxxx;
  vec4 x4 = x0 - 1.0 + 4.0 * C.xxxx;

// Permutations
  i = mod(i, 289.0); 
  float j0 = permute( permute( permute( permute(i.w) + i.z) + i.y) + i.x);
  vec4 j1 = permute( permute( permute( permute (
             i.w + vec4(i1.w, i2.w, i3.w, 1.0 ))
           + i.z + vec4(i1.z, i2.z, i3.z, 1.0 ))
           + i.y + vec4(i1.y, i2.y, i3.y, 1.0 ))
           + i.x + vec4(i1.x, i2.x, i3.x, 1.0 ));
// Gradients
// ( 7*7*6 points uniformly over a cube, mapped onto a 4-octahedron.)
// 7*7*6 = 294, which is close to the ring size 17*17 = 289.

  vec4 ip = vec4(1.0/294.0, 1.0/49.0, 1.0/7.0, 0.0) ;

  vec4 p0 = grad4(j0,   ip);
  vec4 p1 = grad4(j1.x, ip);
  vec4 p2 = grad4(j1.y, ip);
  vec4 p3 = grad4(j1.z, ip);
  vec4 p4 = grad4(j1.w, ip);

// Normalise gradients
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;
  p4 *= taylorInvSqrt(dot(p4,p4));

// Mix contributions from the five corners
  vec3 m0 = max(0.6 - vec3(dot(x0,x0), dot(x1,x1), dot(x2,x2)), 0.0);
  vec2 m1 = max(0.6 - vec2(dot(x3,x3), dot(x4,x4)            ), 0.0);
  m0 = m0 * m0;
  m1 = m1 * m1;
  return 49.0 * ( dot(m0*m0, vec3( dot( p0, x0 ), dot( p1, x1 ), dot( p2, x2 )))
               + dot(m1*m1, vec2( dot( p3, x3 ), dot( p4, x4 ) ) ) ) ;

}

//------------------------UTIL------------------------------------
float map(float value, float min1, float max1, float min2, float max2) {
  return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

//----------------------SDF GEOMETRY------------------------------

float sdfSphere(vec3 point, vec3 center, float r) {    
    return (length(center - point) - r);
}
vec4 sdfSphere(vec3 point, vec3 center, float r, vec3 color) {    
    return vec4((length(center - point) - r),color);
}

float sdfBox(vec3 point, vec3 center, vec3 b) {
  vec3 q = abs(point - center) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float Hash(float p)
{
	vec2 p2 = fract(vec2(p) * MOD2);
    p2 += dot(p2.yx, p2.xy+19.19);
	return fract(p2.x * p2.y);
}
 
//=================================================================================================
float EyeNoise( in float x )
{
    float p = floor(x);
    float f = fract(x);
	f = clamp(pow(f, 7.0), 0.0,1.0);
	//f = f*f*(3.0-2.0*f);
    return mix(Hash(p), Hash(p+1.0), f);
}

//=================================================================================================
float Bump( in vec3 x )
{
    vec3 p = floor(x);
    vec3 f = fract(x);
	//f = f*f*(3.0-2.0*f);
	
	vec2 uv = (p.xy + vec2(37.0, 17.0) * p.z) + f.xy;
	vec2 rg = textureLod( u_texture, (uv+.5)/256., 0.).yx;
	return mix(rg.x, rg.y, f.z);
}

//=================================================================================================
float Noise( in vec3 x )
{
    vec3 p = floor(x);
    vec3 f = fract(x);
	f = f*f*(3.0-2.0*f);
	vec2 uv = (p.xy + vec2(37.0, 17.0) * p.z) + f.xy;
	vec2 rg = textureLod( u_texture, (uv+.5)/256., 0.).yx;
	return mix(rg.x, rg.y, f.z);
}

//=================================================================================================
float Pupil(vec3 p, float r)
{
	// It's just a stretched sphere but the mirrored
	// halves are push together to make a sharper top and bottom.
	p.xz = abs(p.xz)+.25;
	return length(p) - r;
}

//=================================================================================================
float DE_Fire(vec3 p)
{
	p *= vec3(1.0, 1.0, 1.5);
	float len = length(p);
	float ax = atan(p.y, p.x)*10.0;
	float ay = atan(p.y, p.z)*10.0;
	vec3 shape = vec3(len*.5-gTime*1.2, ax, ay) * 2.0;
	
	shape += 2.5 * (Noise(p * .25) -
				 	Noise(p * 0.5) * .5 +
					Noise(p * 2.0) * .25);
	float f = Noise(shape)*6.0;
	f += (LinearStep(7.30, 8.3+flareUp, len)*LinearStep(12.0+flareUp*2.0, 8.0, len)) * 3.0;
	p *= vec3(.75, 1.2, 1.0);
	len = length(p);
	f = mix(f, 0.0, LinearStep(12.5+flareUp, 16.5+flareUp, len));
	return f;
}

//=================================================================================================
float Sphere(vec3 p, float r)
{
	return length(p) - r;
}

//=================================================================================================
float DE_Pillars(vec3 p)
{
	// It's just two spheres with added bumpy noise.
	// Simple, but it'll do fine. :)	
	float d = Sphere((p+vec3(0.0, 1.0, 0.0))*vec3(1.0, 1.0, 18.0), 20.0);
	d = max(-Sphere((p + vec3(0.0, -3.0, 38.0))* vec3(1.5, 1.1, .95), 44.0), d);
	d += Noise(p*2.0)*.15 + Noise(p*8.0)*.04;
	return d;
}

//=================================================================================================
float DE_Pupil(vec3 p)
{
	float time = gTime * .5+sin(gTime*.3)*.5;
	float t = EyeNoise(time) * .125 +.125;
	p.yz = Rotate2axis(p.yz, t * TAU);
	p *= vec3(1.2-EyeNoise(time+32.5)*.5, .155, 1.0);
	t = EyeNoise(time-31.0) * .125 +.1875;
	p.xz = Rotate2axis(p.xz, t*TAU);
	p += vec3(.0, 0.0, 4.);
	
	float  d = Pupil(p, .78);
	return d * max(1.0, abs(p.y*2.5));
}

//=================================================================================================
vec3 Normal( in vec3 pos )
{
	vec2 eps = vec2( 0.1, 0.0);
	vec3 nor = vec3(
	    DE_Pillars(pos+eps.xyy) - DE_Pillars(pos-eps.xyy),
	    DE_Pillars(pos+eps.yxy) - DE_Pillars(pos-eps.yxy),
	    DE_Pillars(pos+eps.yyx) - DE_Pillars(pos-eps.yyx) );
	return normalize(nor);
}


//----------------------SDF OPERATIONS------------------------------

float opUnion(float dist1, float dist2) {
    if (dist1 < dist2) {
        return dist1;
    }
    return dist2;
}
float opSubtraction( float d1, float d2 ) {
    return max(-d1,d2);
}

float opIntersection( float d1, float d2 ) {
    return max(d1,d2);
}

float opSmoothUnion( float d1, float d2, float k ) {
    float h = max(k-abs(d1-d2),0.0);
    return min(d1, d2) - h*h*0.25/k;
}
vec3 FlameColour(float f)
{
	f = f*f*(3.0-2.0*f);
	return  min(vec3(f+.8, f*f*1.4+.05, f*f*f*.6) * f, 1.0);
}
float Sky(vec2 p)
{
	float z = gTime*.5 + 47.5;
	p *= .0025;
	float dist = length(p) * .7;

	float f = 0.0;
	float w = .27;
	for (int i=0; i < 7; i++)
	{
		f += Noise(vec3(p, z)) * w;
		w *= .55;
		p *= 2.7;
	}

	f = smoothstep(.17, 1.0, f)*.55;
	f = f / dist; 
	return f;
}
vec4 Raymarch( in vec3 ro, in vec3 rd, in vec2 fragCoord, inout bool hit, out float pupil)
{
	float sum = 0.0;
	// Starting point plus dither to prevent edge banding...
	float t = 14.0 + .1 * texture(u_texture, fragCoord.xy / u_texture_resolution).y;
	vec3 pos = vec3(0.0, 0.0, 0.0);
	float d = 100.0;
	pupil = 0.0;
	for(int i=0; i < 197; i++)
	{
        if (t > 37.0) break;
		pos = ro + t*rd;
		vec3 shape = pos * vec3(1.5, .4, 1.5);
	
		// Accumulate pixel denisity depending on the distance to the pupil
		d = DE_Pupil(pos);
		pupil += LinearStep(0.02 +Noise(pos*4.0+gTime)*.3, 0.0, d) * .17;

		// Add fire around pupil...
		sum += LinearStep(1.3, 0.0, d) * .014;
		
		// Search for pillars...
		d = DE_Pillars(pos);
		if (d < 0.01)
		{
			pos = ro + (t + d) * rd;
            hit = true;
			break;
		}
		
		sum += max(DE_Fire(pos), 0.0) * .00162;
    	t += max(.1, t*.0057);

	}
	
	return vec4(pos, clamp(sum*sum*sum, 0.0, 1.0 ));
}
//----------------------CREATE YOUR SCENE------------------------
vec4 sdfScene(vec3 position) {
    //Define final distance
    float dist = 0.0;


    return dist;
}

//-----------------------COMPUTE NORMAL SDF POINT------------------
vec3 gradient(float h, vec3 coords) {
    vec3 r = vec3(0.0);
    float grad_x = sdfScene(vec3(coords.x + h, coords.y, coords.z)).x - 
                   sdfScene(vec3(coords.x - h, coords.y, coords.z)).x;

    float grad_y = sdfScene(vec3(coords.x, coords.y + h, coords.z)).x - 
                   sdfScene(vec3(coords.x, coords.y - h, coords.z)).x;
    
    float grad_z = sdfScene(vec3(coords.x, coords.y, coords.z + h)).x - 
                   sdfScene(vec3(coords.x, coords.y, coords.z - h)).x;
    
    return normalize(vec3(grad_x, grad_y, grad_z)  /  (h * 2));
}

//---------------------------SIMPLE PHONG SHADING----------------
vec3 phong(vec3 position) {
    vec3 normal = gradient(0.0001, position);
    vec3 l = normalize( vec3(-1.9, 3.0, 3.0) - position );
    vec3 diff = vec3(0.5) * clamp( dot(l, normal), 0.0, 1.0);
    return diff + vec3(0.1);
}


mat3 setCamera(in vec3 origin, in vec3 target, float rotation) {
    vec3 forward = normalize(target - origin);
    vec3 orientation = vec3(sin(rotation), cos(rotation), 0.0);
    vec3 left = normalize(cross(forward, orientation));
    vec3 up = normalize(cross(left, forward));
    return mat3(left, up, forward);
}


void main()
{
   
    // reconstruct points
    vec2 uv = gl_FragCoord.xy * u_iRes; //extract uvs from pixel screenpos
    //reconstruct world position from depth (near plane depth is 0)
    vec4 screen_pos = vec4((uv.x*2.0-1.0), uv.y*2.0-1.0, 0.0, 1.0);

    gTime = u_time + 44.29;
    flareUp = max(sin(gTime*.75+3.5), 0.0);

	vec2 p = -1.0 + 2.0 * uv;
	p.x *= u_iRes.x/u_iRes.y;

	vec3 origin = vec3(sin(gTime*.34)*5.0, -10.0 - sin(gTime*.415) * 6.0, -20.0+sin(gTime*.15) * 2.0);
	vec3 target = vec3( 0.0, 0.0, 0.0 );
	
	// Make camera ray using origin and target positions...
	vec3 cw = normalize( target-origin);
	vec3 cp = vec3(0.0, 1.0, 0.0);
	vec3 cu = normalize( cross(cw, cp) );
	vec3 cv = ( cross(cu,cw) );
	vec3 ray = normalize(p.x*cu + p.y*cv + 1.5 * cw );
    bool hit = false;
	float pupil = 0.0;
	vec4 ret = Raymarch(origin, ray, screen_pos.xy, hit, pupil);
	vec3 col = vec3(0.0);
    vec3 light = vec3(0.0, 4.0, -4.0);
    // Do the lightning flash effect...
    float t = mod(gTime+3.0, 13.0);
    float flash = smoothstep(0.4, .0, t);
    flash += smoothstep(0.2, .0, abs(t-.6)) * 1.5;
    flash += smoothstep(0.7, .8, t) * smoothstep(1.3, .8, t);
    flash *= 2.2;
if (hit)
	{
		// Pillars...
		vec3 nor  = Normal(ret.xyz);
		vec3 ldir = normalize(light - ret.xyz);
		vec3 ref  = reflect(ray, nor);
		float bri = max(dot(ldir, nor), 0.0) * (1.0+flareUp*2.0) + flash * max(nor.y * 2.0 - nor.z*.5, 0.0);
		float spe = max(dot(ldir, ref), 0.0);
		spe = pow(abs(spe), 40.0) * .15;
		vec3 mat = vec3(.6, .4, .35) * .15;
		col = mat * bri + spe * vec3(.4, .2, .0);
	}else
	{
		// Background...
		if (ray.y > 0.0)
		{
			float d = (250.0 - origin.y) / ray.y;
			vec2 cloud = vec2((ray * d).xz);
			float k = Sky(cloud);
			col = vec3(.7, .7, 1.0) * k;
			col += (smoothstep(0.045, 0.19, k)) * flash * vec3(.58, .53, .6);
		}
	}
	
	col += FlameColour(ret.w);
	col = mix (col, vec3(0.0), min(pupil, 1.0));
	
	// Contrasts...
	col = sqrt(col);
	col = min(mix(vec3(length(col)),col, 1.22), 1.0);
	col += col * .3;
	
	gl_FragColor = vec4(min(col, 1.0),1.0);
}
