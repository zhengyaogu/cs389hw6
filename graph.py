import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

measurements = pd.read_csv("measurements.csv")
sns.distplot(measurements['latency'], hist=True, kde=False,
        color='blue', hist_kws={'edgecolor':'black'})
plt.title('Histogram of Request Latencies')
plt.xlabel('Latency (millisecond)')
plt.ylabel('Frequency')

