#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Tue Oct 15 08:21:46 2019

@author: student
"""

R0 = 1200.0
R2 = 660.0

def f(x):
    volts = (x/1024) * 5.0
    RS = ((5 * R2) / volts) - R2
    ratio = RS / R0
    y = 0.4 * ratio
    return (1/y)**(1.431)

def rs(x):
    volts = (x/1024) * 5.0
    return ((5 * R2) / volts) - R2

def omjer(x):
    volts = (x/1024) * 5.0
    RS = ((5 * R2) / volts) - R2
    return (RS / R0)

import numpy as np
import matplotlib.pyplot as plt

x = np.linspace(200, 900, 1023)

rr = np.array([1.4062, 1.0539, 1.2303, 0.9782, 0.7261, 0.9077, 0.7770, 0.5466, 0.6414, 0.3966, 0.2853, 0.4396])
mgl = np.array([0.257, 0.47, 0.33, 0.43, 0.68, 0.43, 0.57, 1.05, 0.8, 1.7, 2.6, 1.5])

plt.figure()
plt.plot(x, f(x))
plt.title("BAC")
plt.xlabel("ADC")
plt.ylabel("PPT")
plt.grid()


# plt.figure()
# plt.plot(x, rs(x))
# plt.title("RS")
# plt.xlabel("ADC")
# plt.ylabel("RS")
# plt.grid()

# plt.figure()
# plt.plot(x, omjer(x))
# plt.title("RS/R0")
# plt.xlabel("ADC")
# plt.ylabel("RS/R0")
# plt.grid()

plt.figure()
plt.plot(f(x), omjer(x))
plt. plot(mgl * 10, rr, "*")
plt.title("RS/R0 v mg/L")
plt.xlabel("mg/L")
plt.ylabel("RS/R0")
plt.xlim(1, 30)
plt.grid()
