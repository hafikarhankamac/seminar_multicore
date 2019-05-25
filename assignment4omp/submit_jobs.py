import os
import sys


with open('runHEAT.scp', 'r') as f:
    lines = f.readlines()

for i in [1, 2, 4, 8, 12, 16, 24, 32, 48]:
    filename = 'runHeat' + str(i) + '.scp'
    jobname = 'job_' + sys.argv[1] + '_' + str(i) + '.out'
    with open(filename, 'w') as of:
        for line in lines:
            newline = line[:-1]
            if 'job_name' in line:
                newline = line[:-1] + str(i)
            elif 'output' in line:
                newline = '#@ output = ' + jobname
            elif 'error' in line:
                newline = '#@ error = ' + jobname
            elif 'OMP' in line:
                newline = 'export OMP_NUM_THREADS=' + str(i)
            of.write(newline + '\n')
    os.system('llsubmit ' + filename)
