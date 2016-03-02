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
        
        for name in test['attributes'].keys():
            for i in range(test['count']):
                exp = test['expected'][name][i]
                val = test['attributes'][name][i]
                cmd += ' p %s %f %f %d' % (name, val, exp, i)
        
        for name in test.get('uniforms', {}).keys():
            cmd += ' u %s %f' % (name, test['uniforms'][name])
        
        os.system(cmd)
        
        os.remove(".temp")
        os.remove(".temp.bin")
