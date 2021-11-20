import numpy as np
from scipy.optimize import curve_fit
import matplotlib.pyplot as plt

# Fitting function
def fun(x, A, B, C):
    # return A / (x - B)
    return A / x ** C - B


# Distance [cm]
dist = np.array([15, 20, 30, 40, 60, 80, 100, 120, 140, 150])

# Bitval readings [10 bit], sensor Vcc = 5.0 V
bitval = np.array([790, 710, 560, 440, 300, 210, 170, 130, 100, 90])

np.seterr(divide="ignore")
cFit, covar = curve_fit(fun, bitval, dist)  # , p0=init_vals)
fit_A = cFit[0]
fit_B = cFit[1]
fit_C = cFit[2]

print("Fitting results")
print("  distance [cm] = A / bitval ^ C - B")
print("  A: %.2f" % fit_A)
print("  B: %.2f" % fit_B)
print("  C: %.3f\n" % fit_C)

print("dist   bitv    fit")
print("[cm]   2^10   [cm]")
for idx, x in np.ndenumerate(bitval):
    print(
        "%4.0f - %4u - %4u"
        % (
            dist[idx],
            bitval[idx],
            fun(x, fit_A, fit_B, fit_C),
        )
    )

fit_bitval = np.linspace(90, 800, 1024)
fit_dist = fun(fit_bitval, fit_A, fit_B, fit_C)

plt.plot(bitval, dist, "o-k")
plt.plot(fit_bitval, fit_dist, "-r")
plt.title("Calibration IR distance sensor\nSharp 2Y0A02")
plt.xlabel("bit value [10 bits]")
plt.ylabel("distance [cm]")
plt.savefig("IR_sensor_fit.pdf")
plt.show()
