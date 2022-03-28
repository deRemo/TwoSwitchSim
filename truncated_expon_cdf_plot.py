#!/usr/bin/python3

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


#Inverse (doubly) trucated negative exponential cdf
#(As defined in the simulator)
def inv_truncated_expon_CDF(u):
	return A - (log(1 - u * (1 - exp((A - B) / MEAN)))) * MEAN


#Generate random uniform number (Inverse-Transform)
nums = []

for i in range(N):
	u = np.random.uniform(low=0.0, high=1.0)
	nums.append(inv_truncated_expon_CDF(u))

#Plot generated pdf and save figure
plt.hist(nums, bins=BINS, density=True, color="antiquewhite", edgecolor="black")
plt.savefig(f"truncated_expon_pdf.pdf")

#Plot generated cdf and save figure
plt.hist(nums, bins=BINS, density=True, cumulative=True, color="antiquewhite", edgecolor="black")
plt.savefig(f"truncated_expon_cdf.pdf")
