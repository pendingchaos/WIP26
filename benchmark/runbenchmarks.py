import os
import tempfile

files = os.listdir('benchmarks')

os.system('make')

for fname in files:
    benchmarks = eval('['+open('benchmarks/%s'%fname, 'r').read()+']')
    
    for benchmark in benchmarks:
        f = open(".temp", 'w')
        f.write(benchmark['source'])
        f.close()
        
        print 'Running "%s"' % benchmark['name']
        
        os.system('./runbenchmark .temp')
        
        os.remove(".temp")
        os.remove(".temp.bin")
