import os
import tempfile

test_files = os.listdir('tests')

os.system('make')

for testf in test_files:
    testf = eval('['+open('tests/%s'%testf, 'r').read()+']')
    
    for test in testf:
        f = open(".temp", 'w')
        f.write(test['source'])
        f.close()
        
        print 'Running "%s"' % test['name']
        
        cmd = './runtest .temp %d' % (test['count'])
        
        for name in test['properties'].keys():
            for i in range(test['count']):
                exp = test['expected'][name][i]
                val = test['properties'][name][i]
                cmd += ' %s %f %f %d' % (name, val, exp, i)
        
        os.system(cmd)
        
        os.remove(".temp")
        os.remove(".temp.bin")
