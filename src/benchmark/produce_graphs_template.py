#!/usr/bin/python

import matplotlib
matplotlib.use('Agg')

import matplotlib.pyplot as plt

try:
	Format = FORMAT
except NameError:
	Format = 'pdf'

def read_dat_file(the_file):
    with open(the_file, 'r') as f:
        lines = f.readlines()
        x = []
        y = []
        for line in lines:
            p = line.split()
            x.append(float(p[0]))
            y.append(float(p[1])/float(p[2]))
        return (x, y)

def set_up_figure(title):
    plt.figure()
    plt.autoscale(enable=True, tight=False)
    plt.xlabel('Number of Threads')
    plt.ylabel('Microseconds / Operation') 
    plt.title(title)


def plot_file(the_file, title):
    (x_list, y_list) = read_dat_file(the_file)
    mapped = [(a, [b for (comp_a, b) in zip(x_list, y_list) if a == comp_a]) for a in x_list]
    mapped.sort()
    x,y_vals = zip(*mapped)
    y = map(lambda v : sum(v) / float(len(v)), y_vals)
    emin = map(lambda (v, avg) : avg - min(v), zip(y_vals, y))
    emax = map(lambda (v, avg) : max(v) - avg, zip(y_vals, y))
    plt.errorbar(x, y, [emin, emax], label=title, linewidth=2, elinewidth=1, marker='o')
    #plt.plot(x, y, label=title, linewidth=2)


def complete_figure(save_file_name):
    plt.axis(xmin=0)
    plt.axis(ymin=0)
    plt.tight_layout()
    plt.legend(loc='best')
    plt.savefig(save_file_name + '.' + Format, bbox_inches='tight')
    print save_file_name + '.' + Format


