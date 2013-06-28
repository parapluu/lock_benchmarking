#!/usr/bin/python

import matplotlib.pyplot as plt

def read_dat_file(the_file):
    with open(the_file, 'r') as f:
        lines = f.readlines()
        x = []
        y = []
        for line in lines:
            p = line.split()
            x.append(float(p[0]))
            y.append(float(p[1]))
        return (x, y)

def set_up_figure(title):
    plt.figure()
    plt.autoscale(enable=True, tight=False)
    plt.xlabel('Number of Threads')
    plt.ylabel('Microseconds / Operation') 
    plt.title(title)


def plot_file(the_file, title):
    (x_list, y_list) = read_dat_file(the_file)
    plt.plot(x_list, y_list, label=title)


def complete_figure(save_file_name):
    plt.axis(xmin=0)
    plt.axis(ymin=0)
    plt.legend(loc='best')
    plt.savefig(save_file_name + '.pdf', bbox_inches=0)
    print save_file_name + '.pdf'


