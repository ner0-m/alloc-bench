import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import os

import report

mpl.style.use('seaborn')

bytes = report.bytes[0]
threads = report.threads

#  print(data)


def plot_data(title=None):
    ax.plot(bytes,
            data[0],
            linestyle='-',
            label="Hoard malloc",
            color="firebrick")
    ax.plot(bytes,
            data[1],
            linestyle='--',
            label="Hoard free",
            color="firebrick")
    ax.plot(bytes,
            data[2],
            linestyle='-',
            label="TBB malloc",
            color="darkgreen")
    ax.plot(bytes,
            data[3],
            linestyle='--',
            label="TBB free",
            color="darkgreen")

    ax.set_title(title)
    ax.set_xlabel("Allocation size")
    ax.set_ylabel("Time")

    ax.legend(loc="upper left")


for i in [0, 15]:
    # First compare some of the single threaded performance
    data = np.array([
        report.first_allocs[i],
        report.first_frees[i],
        report.first_tbballocs[i],
        report.first_tbbfrees[i],
    ])

    fig, axes = plt.subplots(2, 2)
    fig.suptitle(f'Hoard vs TBB, {i+1} threads')
    ax = axes[0, 0]

    plot_data(title="Test 1")

    ax = axes[0, 1]
    data = np.array([
        report.second_allocs[i],
        report.second_frees[i],
        report.second_tbballocs[i],
        report.second_tbbfrees[i],
    ])
    plot_data(title="Test 2")

    ax = axes[1, 0]
    data = np.array([
        report.third_allocs[i],
        report.third_frees[i],
        report.third_tbballocs[i],
        report.third_tbbfrees[i],
    ])
    plot_data(title="Test 3")

    ax = axes[1, 1]
    data = np.array([
        report.fourth_allocs[i],
        report.fourth_frees[i],
        report.fourth_tbballocs[i],
        report.fourth_tbbfrees[i],
    ])
    plot_data(
        title="Test 4"
    )
    plt.savefig(f"hoard_tbb_{i+1}thr.png")

plt.show()
