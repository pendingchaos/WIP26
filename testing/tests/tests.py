{
    'name': 'empty',
    'source': '',
    'count': 0,
    "attributes": {},
    "expected": {}
},
{
    'name': 'test passthough',
    'source': 'attribute v:float;',
    'count': 3,
    'attributes': {'v.x': [1.0, 2.0, 3.0]},
    'expected': {'v.x': [1.0, 2.0, 3.0]}
},
{
    'name': 'test addition',
    'source':
    '''attribute v:vec3;
    v.x = v.x + 1.0;
    v.y = v.y + 2.0;
    v.z = v.z + 3.0;
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [3.0, 16.0]
    },
    'expected': {
        'v.x': [2.0, 10.0],
        'v.y': [4.0, 15.0],
        'v.z': [6.0, 19.0]
    }
},
{
    'name': 'test subtraction',
    'source':
    '''attribute v:vec3;
    v.x = v.x - 1.0;
    v.y = v.y - 2.0;
    v.z = v.z - 3.0;
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [3.0, 16.0]
    },
    'expected': {
        'v.x': [0.0, 8.0],
        'v.y': [0.0, 11.0],
        'v.z': [0.0, 13.0]
    }
},
{
    'name': 'test multiplication',
    'source':
    '''attribute v:vec3;
    v.x = v.x * 4.0;
    v.y = v.y * 2.0;
    v.z = v.z * 3.0;
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [3.0, 16.0]
    },
    'expected': {
        'v.x': [4.0, 36.0],
        'v.y': [4.0, 26.0],
        'v.z': [9.0, 48.0]
    }
},
{
    'name': 'test division',
    'source':
    '''attribute v:vec3;
    v.x = v.x / 4.0;
    v.y = v.y / 2.0;
    v.z = v.z / 3.0;
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [3.0, 16.0]
    },
    'expected': {
        'v.x': [0.25, 2.25],
        'v.y': [1.0, 6.5],
        'v.z': [1.0, 5.3333]
    }
},
{
    'name': 'test power',
    'source':
    '''attribute v:vec3;
    v.x = v.x ^ 4.0;
    v.y = v.y ^ 2.0;
    v.z = v.z ^ 3.0;
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [3.0, 16.0]
    },
    'expected': {
        'v.x': [1.0, 6561.0],
        'v.y': [4.0, 169.0],
        'v.z': [27.0, 4096.0]
    }
},
{
    'name': 'test get and set member',
    'source':
    '''attribute v:vec3;
    var v2:vec3 = v;
    v.x = v2.z;
    v.y = v2.x;
    v.z = v2.y;
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [3.0, 16.0]
    },
    'expected': {
        'v.x': [3.0, 16.0],
        'v.y': [1.0, 9.0],
        'v.z': [2.0, 13.0]
    }
},
{
    'name': 'test swizzle',
    'source':
    '''attribute v:vec3;
    v.xzy = v.yzx;
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [3.0, 16.0]
    },
    'expected': {
        'v.x': [2.0, 13.0],
        'v.y': [1.0, 9.0],
        'v.z': [3.0, 16.0]
    }
},
{
    'name': 'test min',
    'source':
    '''include stdlib;
    attribute v:vec3;
    var v2:vec3 = v;
    v.x = min(v2.x, v2.y);
    v.y = min(v2.y, v2.z);
    v.z = min(v2.z, v2.x);
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [3.0, 16.0]
    },
    'expected': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [1.0, 9.0]
    }
},
{
    'name': 'test max',
    'source':
    '''include stdlib;
    attribute v:vec3;
    var v2:vec3 = v;
    v.x = max(v2.x, v2.y);
    v.y = max(v2.y, v2.z);
    v.z = max(v2.z, v2.x);
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [3.0, 16.0]
    },
    'expected': {
        'v.x': [2.0, 13.0],
        'v.y': [3.0, 16.0],
        'v.z': [3.0, 16.0]
    }
},
{
    'name': 'test sqrt',
    'source':
    '''include stdlib;
    attribute v:vec3;
    v.x = sqrt(v.x);
    v.y = sqrt(v.y);
    v.z = sqrt(v.z);
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [3.0, 16.0]
    },
    'expected': {
        'v.x': [1.0, 3.0],
        'v.y': [1.41421, 3.60555],
        'v.z': [1.73205, 4.0]
    }
},
{
    'name': 'test saturate',
    'source':
    '''include stdlib;
    attribute v:vec3;
    v.x = saturate(v.x);
    v.y = saturate(v.y);
    v.z = saturate(v.z);
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, -9.0],
        'v.y': [0.75, 0.25],
        'v.z': [3.0, -16.0]
    },
    'expected': {
        'v.x': [1.0, 0.0],
        'v.y': [0.75, 0.25],
        'v.z': [1.0, 0.0]
    }
},
{
    'name': 'test functions',
    'source':
    '''attribute v:vec3;
    func f1(v:float):float {
        return v*2.0 - 1.0;
    }
    func f2(v:float):float {
        return v;
    }
    func f3(v:float):float {
        return f1(v) + 2.0;
    }
    v.x = f1(v.x);
    v.y = f2(v.y);
    v.z = f3(v.z);
    ''',
    'count': 2,
    'attributes': {
        'v.x': [1.0, 9.0],
        'v.y': [2.0, 13.0],
        'v.z': [3.0, 16.0]
    },
    'expected': {
        'v.x': [1.0, 17.0],
        'v.y': [2.0, 13.0],
        'v.z': [7.0, 33.0]
    }
},
{
    'name': 'test select',
    'source':
    '''include stdlib;
    attribute v:vec2;
    v.x = sel(1.0, 0.0, false);
    v.y = sel(1.0, 0.0, true);
    ''',
    'count': 1,
    'attributes': {
        'v.x': [5.0],
        'v.y': [5.0]
    },
    'expected': {
        'v.x': [0.0],
        'v.y': [1.0]
    }
},
{
    'name': 'test &&',
    'source':
    '''include stdlib;
    attribute v:vec4;
    v.x = sel(1.0, 0.0, false && false);
    v.y = sel(1.0, 0.0, false && true);
    v.z = sel(1.0, 0.0, true && false);
    v.w = sel(1.0, 0.0, true && true);
    ''',
    'count': 1,
    'attributes': {
        'v.x': [5.0],
        'v.y': [5.0],
        'v.z': [5.0],
        'v.w': [5.0]
    },
    'expected': {
        'v.x': [0.0],
        'v.y': [0.0],
        'v.z': [0.0],
        'v.w': [1.0]
    }
},
{
    'name': 'test ||',
    'source':
    '''include stdlib;
    attribute v:vec4;
    v.x = sel(1.0, 0.0, false || false);
    v.y = sel(1.0, 0.0, false || true);
    v.z = sel(1.0, 0.0, true || false);
    v.w = sel(1.0, 0.0, true || true);
    ''',
    'count': 1,
    'attributes': {
        'v.x': [5.0],
        'v.y': [5.0],
        'v.z': [5.0],
        'v.w': [5.0]
    },
    'expected': {
        'v.x': [0.0],
        'v.y': [1.0],
        'v.z': [1.0],
        'v.w': [1.0]
    }
},
{
    'name': 'test !',
    'source':
    '''include stdlib;
    attribute v:vec2;
    v.x = sel(1.0, 0.0, !false);
    v.y = sel(1.0, 0.0, !true);
    ''',
    'count': 1,
    'attributes': {
        'v.x': [5.0],
        'v.y': [5.0]
    },
    'expected': {
        'v.x': [1.0],
        'v.y': [0.0]
    }
},
{
    'name': 'test <',
    'source':
    '''include stdlib;
    attribute v:vec2;
    v.x = sel(1.0, 0.0, 2 < 3);
    v.y = sel(1.0, 0.0, 3 < 2);
    ''',
    'count': 1,
    'attributes': {
        'v.x': [5.0],
        'v.y': [5.0]
    },
    'expected': {
        'v.x': [1.0],
        'v.y': [0.0]
    }
},
{
    'name': 'test >',
    'source':
    '''include stdlib;
    attribute v:vec2;
    v.x = sel(1.0, 0.0, 3 > 2);
    v.y = sel(1.0, 0.0, 2 > 3);
    ''',
    'count': 1,
    'attributes': {
        'v.x': [5.0],
        'v.y': [5.0]
    },
    'expected': {
        'v.x': [1.0],
        'v.y': [0.0]
    }
},
{
    'name': 'test ==',
    'source':
    '''include stdlib;
    attribute v:vec2;
    v.x = sel(1.0, 0.0, 2 == 2);
    v.y = sel(1.0, 0.0, 2 == 3);
    ''',
    'count': 1,
    'attributes': {
        'v.x': [5.0],
        'v.y': [5.0]
    },
    'expected': {
        'v.x': [1.0],
        'v.y': [0.0]
    }
},
{
    'name': 'test if',
    'source':
    '''include stdlib;
    attribute v:vec4;
    if false {v.x = 7.0;}
    if true {v.y = 9.0;}
    v.z = 2.0;
    if true {v.z = 4.0;}
    if length(v) > 11.0 {v.w = 1.0;}
    ''',
    'count': 1,
    'attributes': {
        'v.x': [3.0],
        'v.y': [6.0],
        'v.z': [9.0],
        'v.w': [12.0]
    },
    'expected': {
        'v.x': [3.0],
        'v.y': [9.0],
        'v.z': [4.0],
        'v.w': [1.0]
    }
},
{
    'name': 'test if2',
    'source':
    '''attribute v:vec4;
    if true {v.x = 7.0;v.y = 2.0;}
    if false {v.z = 9.0;v.w = 3.0;}
    ''',
    'count': 1,
    'attributes': {
        'v.x': [3.0],
        'v.y': [6.0],
        'v.z': [7.0],
        'v.w': [8.0]
    },
    'expected': {
        'v.x': [7.0],
        'v.y': [2.0],
        'v.z': [7.0],
        'v.w': [8.0]
    }
},
{
    'name': 'test if3',
    'source':
    '''attribute v:vec4;
    if true {v.x = v.y;}
    if false {v.z = v.w;}
    ''',
    'count': 1,
    'attributes': {
        'v.x': [3.0],
        'v.y': [6.0],
        'v.z': [7.0],
        'v.w': [8.0]
    },
    'expected': {
        'v.x': [6.0],
        'v.y': [6.0],
        'v.z': [7.0],
        'v.w': [8.0]
    }
},
{
    'name': 'test uniforms',
    'source':
    '''attribute v:float;
    uniform nv:float;
    v = nv;
    ''',
    'count': 1,
    'attributes': {
        'v.x': [3.0],
    },
    'expected': {
        'v.x': [7.0],
    },
    'uniforms': {
        'nv.x': 7.0
    }
},
{
    'name': 'test loops',
    'source':
    '''attribute v:vec2;
    while v.x < 5 {
        v.x = v.x + 1;
    }
    for var i:float; i<5; i=i+1 {
        v.y = v.y + 1;
    }
    ''',
    'count': 1,
    'attributes': {
        'v.x': [1.0],
        'v.y': [1.0]
    },
    'expected': {
        'v.x': [5.0],
        'v.y': [5.0]
    }
},
{
    'name': 'test floor, ceil and fract',
    'source':
    '''include stdlib;
    attribute v:vec3;
    v.x = floor(v.x);
    v.y = ceil(v.y);
    v.z = fract(v.z);
    ''',
    'count': 1,
    'attributes': {
        'v.x': [1.5],
        'v.y': [1.5],
        'v.z': [1.5]
    },
    'expected': {
        'v.x': [1.0],
        'v.y': [2.0],
        'v.z': [0.5]
    }
},
{
    'name': 'test clamp',
    'source':
    '''include stdlib;
    attribute v:vec3;
    v.x = clamp(v.x, 3, 7);
    v.y = clamp(v.y, 3, 7);
    v.z = clamp(v.z, 3, 7);
    ''',
    'count': 1,
    'attributes': {
        'v.x': [1.0],
        'v.y': [5.0],
        'v.z': [9.0]
    },
    'expected': {
        'v.x': [3.0],
        'v.y': [5.0],
        'v.z': [7.0]
    }
},
{
    'name': 'test round',
    'source':
    '''include stdlib;
    attribute v:vec3;
    v.x = round(v.x);
    v.y = round(v.y);
    v.z = round(v.z);
    ''',
    'count': 1,
    'attributes': {
        'v.x': [1.6],
        'v.y': [1.3],
        'v.z': [9.0]
    },
    'expected': {
        'v.x': [2.0],
        'v.y': [1.0],
        'v.z': [9.0]
    }
}
