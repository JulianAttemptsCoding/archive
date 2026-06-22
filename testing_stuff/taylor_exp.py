import numpy as np
import matplotlib.pyplot as plt
from math import factorial

def taylor_exp(x, n_terms):
    """
    Calculate e^x using Taylor series expansion
    e^x = sum(x^n / n!) for n from 0 to infinity
    """
    result = 0
    for n in range(n_terms):
        result += x**n / factorial(n)
    return result

def taylor_exp_vectorized(x_array, n_terms):
    """
    Vectorized version for numpy arrays
    """
    return np.array([taylor_exp(x, n_terms) for x in x_array])

# Create x values
x = np.linspace(-3, 3, 1000)

# Calculate actual e^x
y_actual = np.exp(x)

# Calculate Taylor series approximations with different numbers of terms
terms_list = [3, 5, 10, 15]
colors = ['red', 'green', 'blue', 'orange']

# Create the plot
plt.figure(figsize=(12, 8))

# Plot actual e^x
plt.plot(x, y_actual, 'black', linewidth=2, label='e^x (actual)', zorder=5)

# Plot Taylor series approximations
for i, n_terms in enumerate(terms_list):
    y_taylor = taylor_exp_vectorized(x, n_terms)
    plt.plot(x, y_taylor, colors[i], linewidth=1.5, linestyle='--', 
             label=f'Taylor series ({n_terms} terms)', alpha=0.8)

# Formatting
plt.title('e^x vs Taylor Series Approximations', fontsize=16)
plt.xlabel('x', fontsize=14)
plt.ylabel('y', fontsize=14)
plt.legend(fontsize=12)
plt.grid(True, alpha=0.3)
plt.xlim(-3, 3)
plt.ylim(-2, 20)

# Add annotation (simplified to avoid LaTeX issues)
plt.text(0.5, 15, 'Taylor series: e^x = sum(x^n / n!) for n=0 to infinity', 
         fontsize=10, bbox=dict(boxstyle="round,pad=0.3", facecolor="lightblue"))

plt.tight_layout()
plt.show()

# Print some numerical comparisons
print("Comparison at x=1:")
print(f"Actual e^1 = {np.exp(1):.6f}")
for n_terms in terms_list:
    taylor_val = taylor_exp(1, n_terms)
    error = abs(np.exp(1) - taylor_val)
    print(f"Taylor ({n_terms:2d} terms) = {taylor_val:.6f}, Error = {error:.6f}")
