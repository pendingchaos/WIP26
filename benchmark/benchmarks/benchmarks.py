{
    'name': 'addition',
    'source':
    '''attribute position:float;
    attribute velocity:float;
    position = position + velocity;
    position = position + 1.0;
    position = position + velocity;
    position = position + 1.0;
    position = position + velocity;
    position = position + 1.0;
    position = position + velocity;
    position = position + 1.0;
    position = position + velocity;
    position = position + 1.0;
    '''
},
{
    'name': 'subtraction',
    'source':
    '''attribute position:float;
    attribute velocity:float;
    position = position - velocity;
    position = position - 1.0;
    position = position - velocity;
    position = position - 1.0;
    position = position - velocity;
    position = position - 1.0;
    position = position - velocity;
    position = position - 1.0;
    position = position - velocity;
    position = position - 1.0;
    '''
},
{
    'name': 'multiplication',
    'source':
    '''attribute position:float;
    attribute velocity:float;
    position = position * velocity;
    position = position * 1.0;
    position = position * velocity;
    position = position * 1.0;
    position = position * velocity;
    position = position * 1.0;
    position = position * velocity;
    position = position * 1.0;
    position = position * velocity;
    position = position * 1.0;
    '''
},
{
    'name': 'division',
    'source':
    '''attribute position:float;
    attribute velocity:float;
    position = position / velocity;
    position = position / 1.0;
    position = position / velocity;
    position = position / 1.0;
    position = position / velocity;
    position = position / 1.0;
    position = position / velocity;
    position = position / 1.0;
    position = position / velocity;
    position = position / 1.0;
    '''
},
{
    'name': 'power',
    'source':
    '''attribute position:float;
    attribute velocity:float;
    position = position ^ velocity;
    position = position ^ 1.0;
    position = position ^ velocity;
    position = position ^ 1.0;
    position = position ^ velocity;
    position = position ^ 1.0;
    position = position ^ velocity;
    position = position ^ 1.0;
    position = position ^ velocity;
    position = position ^ 1.0;
    '''
},
{
    'name': 'sqrt',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    position = sqrt(position);
    position = sqrt(position);
    position = sqrt(position);
    position = sqrt(position);
    position = sqrt(position);
    position = sqrt(position);
    position = sqrt(position);
    position = sqrt(position);
    position = sqrt(position);
    position = sqrt(position);
    '''
},
{
    'name': 'floor',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    position = floor(position);
    position = floor(position);
    position = floor(position);
    position = floor(position);
    position = floor(position);
    position = floor(position);
    position = floor(position);
    position = floor(position);
    position = floor(position);
    position = floor(position);
    '''
},
{
    'name': 'sel',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    position = sel(position, velocity, true);
    position = sel(position, velocity, false);
    position = sel(position, velocity, true);
    position = sel(position, velocity, false);
    position = sel(position, velocity, true);
    position = sel(position, velocity, false);
    position = sel(position, velocity, true);
    position = sel(position, velocity, false);
    position = sel(position, velocity, true);
    position = sel(position, velocity, false);
    '''
},
{
    'name': '>',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    position > velocity;
    position > velocity;
    position > velocity;
    position > velocity;
    position > velocity;
    position > velocity;
    position > velocity;
    position > velocity;
    position > velocity;
    position > velocity;
    '''
},
{
    'name': '<',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    position < velocity;
    position < velocity;
    position < velocity;
    position < velocity;
    position < velocity;
    position < velocity;
    position < velocity;
    position < velocity;
    position < velocity;
    position < velocity;
    '''
},
{
    'name': '==',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    position == velocity;
    position == velocity;
    position == velocity;
    position == velocity;
    position == velocity;
    position == velocity;
    position == velocity;
    position == velocity;
    position == velocity;
    position == velocity;
    '''
},
{
    'name': '&&',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    var t:bool = true;
    var f:bool = false;
    t && t;
    t && f;
    f && t;
    f && f;
    t && t;
    t && f;
    f && t;
    f && f;
    t && t;
    t && f;
    '''
},
{
    'name': '||',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    var t:bool = true;
    var f:bool = false;
    t || t;
    t || f;
    f || t;
    f || f;
    t || t;
    t || f;
    f || t;
    f || f;
    t || t;
    t || f;
    '''
},
{
    'name': '!',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    var v:bool = !!!!!!!!!!false;
    '''
},
{
    'name': 'if',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    var t:bool = true;
    var f:bool = false;
    if t {}
    if f {}
    if t {}
    if f {}
    if t {}
    if f {}
    if t {}
    if f {}
    if t {}
    if f {}
    '''
},
{
    'name': 'movef',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    position = 1.0;
    velocity = 2.0;
    position = 1.0;
    velocity = 2.0;
    position = 1.0;
    velocity = 2.0;
    position = 1.0;
    velocity = 2.0;
    position = 1.0;
    velocity = 2.0;
    '''
},
{
    'name': 'loop',
    'source':
    '''include stdlib;
    attribute position:float;
    attribute velocity:float;
    var f:bool = false;
    while f {}
    while f {}
    while f {}
    while f {}
    while f {}
    while f {}
    while f {}
    while f {}
    while f {}
    while f {}
    '''
}
