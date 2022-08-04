#!/usr/bin/python3

#Visualize the doubly trucated negative exponential distribution

from math import log,exp
import numpy as np
import matplotlib.pyplot as plt

#fixed values
N = 100000
BINS = 40

A = 64
B = 1500
MEAN = 782

#plot config
plt.rc('xtick', labelsize=18)
plt.rc('ytick', labelsize=12)


def inv_expon_CDF(u):
    return -MEAN * log(u);

#Inverse (doubly) trucated negative exponential cdf
#(As defined in the simulator)
#Derived by me starting by a cdf found online
def inv_truncated_expon_CDF1(u):
    return A - MEAN * (log(1 - (u * (1 - exp((A - B) / MEAN)))))


#Completely derived by me
def inv_truncated_expon_CDF2(u):
    return -MEAN * log(exp(-A/MEAN) - (exp(-A/MEAN) - exp(-B/MEAN)) * u)

#Generate random uniform number (Inverse-Transform)
for inv_f,name in zip([inv_expon_CDF, inv_truncated_expon_CDF1, inv_truncated_expon_CDF2], ["inv_expon", "inv_t_expon1", "inv_t_expon2"]):
    nums = []

    for i in range(N):
        u = np.random.uniform(low=0.0, high=1.0)

        nums.append(inv_f(u))
    
    #Plot generated pdf and save figure
    plt.hist(nums, bins=BINS, density=True, color="antiquewhite", edgecolor="black")
    plt.title(f"{name} pdf")
    plt.savefig(f"{name}_pdf_plot.pdf")
    plt.show()

    #Plot generated cdf and save figure
    plt.hist(nums, bins=BINS, density=True, cumulative=True, color="antiquewhite", edgecolor="black")
    plt.title(f"{name} cdf")
    plt.savefig(f"{name}_cdf_plot.pdf")
    plt.show()