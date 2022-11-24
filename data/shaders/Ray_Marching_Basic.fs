//By Arnau Colom, Juan S. Marquerie

//Optional to use
uniform vec4 u_color;
uniform float u_time;

uniform mat4 u_inverse_viewprojection;
uniform mat4 u_viewprojection;

uniform vec2 u_iRes;
uniform vec3 u_camera_pos;

//Edit
float random (in vec2 st) {
    return fract(sin(dot(st.xy,
                         vec2(12.9898,78.233)))
                 * 43758.5453123);
}

//-------------------------PERLIN NOISE-----------------
//  Simplex 4D Noise 
//  by Ian McEwan, Ashima Arts
//
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
float permute(float x){return floor(mod(((x*34.0)+1.0)*x, 289.0));}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
float taylorInvSqrt(float r){return 1.79284291400159 - 0.85373472095314 * r;}

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

float sdfBox(vec3 point, vec3 center, vec3 b) {
  vec3 q = abs(point - center) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

//----------------------SDF OPERATIONS------------------------------

float opUnion(float dist1, float dist2) {
    if (dist1 < dist2) {
        return dist1;
    }
    return dist2;
}

//----------------------CREATE YOUR SCENE------------------------
float sdfScene(vec3 position) {
    //Define final distance
    float dist = 0.0; 
    //Define sphere
    vec3 sphere_pos = vec3(0.0); float sphere_rad = 0.25;
    float dist_esphere = sdfSphere(position,sphere_pos,sphere_rad); 
 

    dist = dist_esphere;
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

    vec4 proj_worldpos = u_inverse_viewprojection * screen_pos;
    vec3 worldpos = proj_worldpos.xyz / proj_worldpos.w;


     //Ray
    vec3 origen = u_camera_pos;
    vec3 dir = normalize(worldpos-u_camera_pos);
    vec3 pos = origen + dir;//(v_position+1.0)/2.0;

    vec4 acc_color = vec4(0.);


    for(int i=0; i<100; i++){
        float min_length = 0;       
        min_length = sdfScene(pos);
        // HIT!! 
        if (min_length < 0.001) {                              
            acc_color = vec4(phong(pos), 1.0);
            break;
        } 

        //Calculem la nova posició
        pos += dir*min_length;              
    }

    //Modifiquem el color final segons la brillantor
    gl_FragColor = acc_color;
}
