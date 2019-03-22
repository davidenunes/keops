"""
Scaling up Gaussian convolutions on 3D point clouds
===========================================================

Let's compare the performances of PyTorch and KeOps on 
simple Gaussian RBF kernel products,
as the number of samples grows from 100 to 1,000,000.

.. note::
    In this demo, we use exact **bruteforce** computations 
    (tensorized for PyTorch and online for KeOps), without leveraging any multiscale
    or low-rank (multipole) decomposition of the Kernel matrix.
    Please visit the documentation of the `GeomLoss package<https://www.kernel-operations.io/geomloss>`_
    for a discussion of alternate, scalable schemes.
"""


##############################################
# Setup
# ---------------------

import os
import numpy as np
import time
from matplotlib import pyplot as plt

import importlib
import torch

use_cuda = torch.cuda.is_available()

##############################################
# Benchmark specifications:
# 

D  = 3        # Let's do this in 3D
MAXTIME = 20 if use_cuda else 1   # Max number of seconds before we break the loop
REDTIME = 2  if use_cuda else .2  # Decrease the number of runs if computations take longer than 2s...

# Number of samples that we'll loop upon
NS = [100, 200, 500, 
      1000, 2000, 5000, 
      10000, 20000, 50000, 
      100000, 200000, 500000,
      1000000]


##############################################
# Synthetic dataset. Feel free to use
# a Stanford Bunny, or whatever!

def generate_samples(N, device):
    """Create point clouds sampled non-uniformly on a sphere of diameter 1."""

    x  = torch.randn(N, D, device=device)
    x[:,0] += 1
    x  = x / (2*x.norm(dim=1,keepdim=True))

    y  = torch.randn(N, D, device=device)
    y[:,1] += 2
    y  = y / (2*y.norm(dim=1,keepdim=True))

    # Draw a random source signal:
    b  = torch.randn(N, device=device).view(-1,1)

    return x, y, b


##############################################
# Define a simple Gaussian RBF product, using a **tensorized** implementation:
#

def gaussianconv_pytorch(x, y, b):
    D_xx = (x*x).sum(-1).unsqueeze(1)         # (N,1)
    D_xy = torch.matmul( x, y.permute(1,0) )  # (N,D) @ (D,M) = (N,M)
    D_yy = (y*y).sum(-1).unsqueeze(0)         # (1,M)
    D_xy = D_xx - 2*D_xy + D_yy
    K_xy = (-D_xy).exp()

    return K_xy @ b.view(-1,1)

##############################################
# Define a simple Gaussian RBF product, using an **online** implementation:
#

from pykeops.torch import generic_sum

gaussianconv_keops = generic_sum("Exp(-SqDist(X,Y)) * B",  # Formula
                                 "A = Vx(1)",              # Output
                                 "X = Vx({})".format(D),   # 1st argument
                                 "Y = Vy({})".format(D),   # 2nd argument
                                 "B = Vy(1)" )             # 3rd argument


##############################################
# Benchmarking loops
# -----------------------

def benchmark(Routine, dev, N, loops = 10) :
    """Times a convolution on an N-by-N problem."""

    importlib.reload(torch)  # In case we had a memory overflow just before...
    device = torch.device(dev)
    x, y, b = generate_samples(N, device)

    # We simply benchmark a convolution
    code = "a = Routine( x, y, b ) "
    exec( code, locals() ) # Warmup run, to compile and load everything

    t_0 = time.perf_counter()  # Actual benchmark --------------------
    if use_cuda: torch.cuda.synchronize()
    for i in range(loops):
        exec( code, locals() )
    if use_cuda: torch.cuda.synchronize()
    elapsed = time.perf_counter() - t_0  # ---------------------------

    print("{:3} NxN convolution, with N ={:7}: {:3}x{:3.6f}s".format(loops, N, loops, elapsed / loops))
    return elapsed / loops


def bench_config(Routine, backend, dev) :
    """Times a convolution for an increasing number of samples."""

    print("Backend : {}, Device : {} -------------".format(backend, dev))

    times = []
    try :
        Nloops = [100, 10, 1]
        nloops = Nloops.pop(0)
        for n in NS :
            elapsed = benchmark(Routine, dev, n, loops=nloops)

            times.append( elapsed )
            if (nloops * elapsed > MAXTIME) \
            or (nloops * elapsed > REDTIME/10 and len(Nloops) > 0 ) : 
                nloops = Nloops.pop(0)

    except RuntimeError :
        print("**\nMemory overflow !")
    except IndexError :
        print("**\nToo slow !")
    
    return times + (len(NS)-len(times)) * [np.nan]


def full_bench(title, routines) :
    """Benchmarks the varied backends of a geometric loss function."""

    backends = [ backend for (_,backend) in routines ]

    print("Benchmarking : {} ===============================".format(title))
    
    lines  = [ NS ]
    for routine, backend in routines :
        lines.append( bench_config(routine, backend, "cuda" if use_cuda else "cpu") )

    benches = np.array(lines).T

    # Creates a pyplot figure:
    plt.figure(figsize=(12,8))
    linestyles = ["o-", "s-", "^-"]
    for i, backend in enumerate(backends):
        plt.plot( benches[:,0], benches[:,i+1], linestyles[i], 
                  linewidth=2, label='backend = "{}"'.format(backend) )
        
        for (j, val) in enumerate( benches[:,i+1] ):
            if np.isnan(val) and j > 0:
                x, y = benches[j-1,0], benches[j-1,i+1]
                plt.annotate('Memory overflow!',
                    xy=(x, 1.05*y),
                    horizontalalignment='center',
                    verticalalignment='bottom')
                break

    plt.title('Runtime for {} in dimension {}'.format(title, D))
    plt.xlabel('Number of samples')
    plt.ylabel('Seconds')
    plt.yscale('log') ; plt.xscale('log')
    plt.legend(loc='upper left')
    plt.grid(True, which="major", linestyle="-")
    plt.grid(True, which="minor", linestyle="dotted")
    plt.axis([ NS[0], NS[-1], 1e-4, MAXTIME ])
    plt.tight_layout()

    # Save as a .csv to put a nice Tikz figure in the papers:
    header = "Npoints " + " ".join(backends)
    os.makedirs("output", exist_ok=True)
    np.savetxt("output/benchmark_convolutions_3D.csv", benches, 
               fmt='%-9.5f', header=header, comments='')


##############################################
# Run the benchmark
# ---------------------

routines = [ (gaussianconv_pytorch, "PyTorch"),  
             (gaussianconv_keops,   "KeOps")  ]
full_bench( "Gaussian Matrix-Vector products", routines )

plt.show()