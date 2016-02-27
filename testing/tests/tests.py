{
    'name': 'empty',
    'source': '',
    'count': 0,
    "properties": {},
    "expected": {}
},
{
    'name': 'passthough',
    'source': 'property v:float;',
    'count': 3,
    'properties': {'v.x': [1.0, 2.0, 3.0]},
    'expected': {'v.x': [1.0, 2.0, 3.0]}
},
{
    'name': 'addition',
    'source':
    '''property v:vec3;
    v.x = v.x + 1.0;
    v.y = v.y + 2.0;
    v.z = v.z + 3.0;
    ''',
    'count': 2,
    'properties': {
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
    'name': 'subtraction',
    'source':
    '''property v:vec3;
    v.x = v.x - 1.0;
    v.y = v.y - 2.0;
    v.z = v.z - 3.0;
    ''',
    'count': 2,
    'properties': {
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
    'name': 'multiplication',
    'source':
    '''property v:vec3;
    v.x = v.x * 4.0;
    v.y = v.y * 2.0;
    v.z = v.z * 3.0;
    ''',
    'count': 2,
    'properties': {
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
    'name': 'division',
    'source':
    '''property v:vec3;
    v.x = v.x / 4.0;
    v.y = v.y / 2.0;
    v.z = v.z / 3.0;
    ''',
    'count': 2,
    'properties': {
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
    'name': 'power',
    'source':
    '''property v:vec3;
    v.x = v.x ^ 4.0;
    v.y = v.y ^ 2.0;
    v.z = v.z ^ 3.0;
    ''',
    'count': 2,
    'properties': {
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
    'name': 'get and set member',
    'source':
    '''property v:vec3;
    var v2:vec3 = v;
    v.x = v2.z;
    v.y = v2.x;
    v.z = v2.y;
    ''',
    'count': 2,
    'properties': {
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
    'name': 'swizzle',
    'source':
    '''property v:vec3;
    v.xzy = v.yzx;
    ''',
    'count': 2,
    'properties': {
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
    property v:vec3;
    var v2:vec3 = v;
    v.x = min(v2.x, v2.y);
    v.y = min(v2.y, v2.z);
    v.z = min(v2.z, v2.x);
    ''',
    'count': 2,
    'properties': {
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
    property v:vec3;
    var v2:vec3 = v;
    v.x = max(v2.x, v2.y);
    v.y = max(v2.y, v2.z);
    v.z = max(v2.z, v2.x);
    ''',
    'count': 2,
    'properties': {
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
    property v:vec3;
    v.x = sqrt(v.x);
    v.y = sqrt(v.y);
    v.z = sqrt(v.z);
    ''',
    'count': 2,
    'properties': {
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
    property v:vec3;
    v.x = saturate(v.x);
    v.y = saturate(v.y);
    v.z = saturate(v.z);
    ''',
    'count': 2,
    'properties': {
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
    '''property v:vec3;
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
    'properties': {
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
    property v:vec2;
    v.x = sel(1.0, 0.0, false);
    v.y = sel(1.0, 0.0, true);
    ''',
    'count': 1,
    'properties': {
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
    property v:vec4;
    v.x = sel(1.0, 0.0, false && false);
    v.y = sel(1.0, 0.0, false && true);
    v.z = sel(1.0, 0.0, true && false);
    v.w = sel(1.0, 0.0, true && true);
    ''',
    'count': 1,
    'properties': {
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
    property v:vec4;
    v.x = sel(1.0, 0.0, false || false);
    v.y = sel(1.0, 0.0, false || true);
    v.z = sel(1.0, 0.0, true || false);
    v.w = sel(1.0, 0.0, true || true);
    ''',
    'count': 1,
    'properties': {
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
    property v:vec2;
    v.x = sel(1.0, 0.0, !false);
    v.y = sel(1.0, 0.0, !true);
    ''',
    'count': 1,
    'properties': {
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
    property v:vec2;
    v.x = sel(1.0, 0.0, 2 < 3);
    v.y = sel(1.0, 0.0, 3 < 2);
    ''',
    'count': 1,
    'properties': {
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
    property v:vec2;
    v.x = sel(1.0, 0.0, 3 > 2);
    v.y = sel(1.0, 0.0, 2 > 3);
    ''',
    'count': 1,
    'properties': {
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
    property v:vec2;
    v.x = sel(1.0, 0.0, 2 == 2);
    v.y = sel(1.0, 0.0, 2 == 3);
    ''',
    'count': 1,
    'properties': {
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
    '''
    property v:vec2;
    if false {v.x = 7.0;}
    if true {v.y = 9.0;}
    ''',
    'count': 1,
    'properties': {
        'v.x': [3.0],
        'v.y': [6.0]
    },
    'expected': {
        'v.x': [3.0],
        'v.y': [9.0]
    }
}
