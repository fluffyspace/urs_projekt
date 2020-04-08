#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Tue Oct 15 08:21:46 2019

@author: student
"""

def f(x):
    R0 = 10000.0
    R2 = 1000.0
    volts = (x/1024) * 5.0
    RS = ((5 * R2) / volts) - R2
    ratio = RS / R0
    y = 0.4 * ratio
    return (1/y)**(1.431)

import numpy as np
import matplotlib.pyplot as plt

x = np.linspace(1, 900, 1023)

plt.figure()
plt.plot(x, f(x))
plt.title("hello there")
plt.xlabel("ADC")
plt.ylabel("PPT")
plt.grid()
