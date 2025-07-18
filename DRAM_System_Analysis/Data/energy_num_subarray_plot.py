import json
import pandas as pd
import matplotlib.pyplot as plt
import re
import numpy as np
from matplotlib.patches import Patch
from matplotlib.colors import LinearSegmentedColormap

# Load the JSON data into a DataFrame
def load_trace_summary(json_path):
    with open(json_path, 'r') as f:
        data = json.load(f)
    return pd.DataFrame(data)

# Extract Ndbl size (e.g., from "UCA1_Ndbl_32AR" → 32)
def extract_ndbl_size(name):
    match = re.search(r'_Ndbl_(\d+)', name)
    return int(match.group(1)) if match else None

# Plot total energy for Auto Refresh group only
def plot_total_energy_auto_refresh(df):
    # Identify group and subarray
    df['Group'] = df['name'].apply(lambda x: 'Auto Refresh' if 'AR' in x else 'WUPR')
    df = df[df['Group'] == 'Auto Refresh'].copy()

    # Extract and filter subarray count
    df['Subarray'] = df['name'].apply(extract_ndbl_size)
    df = df[df['Subarray'] != 32]

    # Convert to mJ
    df['total_energy_mJ'] = df['total_energy'] / 1e6

    # Sort by Subarray
    df = df.sort_values(by='Subarray').reset_index(drop=True)

    # Unique subarray values
    subarray_vals = sorted(df['Subarray'].dropna().unique())
    cmap = LinearSegmentedColormap.from_list("purple_yellow", ["#5e3c99", "#b2abd2", "#fdb863", "#e66101"], N=len(subarray_vals))
    color_list = [cmap(i / (len(subarray_vals) - 1)) for i in range(len(subarray_vals))]
    color_palette = dict(zip(subarray_vals, color_list))

    # Color mapping
    df['Color'] = df['Subarray'].map(color_palette).fillna('#ffffff')

    # Plotting
    plt.figure(figsize=(12, 6))
    bar_positions = list(range(len(df)))
    bar_colors = df['Color'].tolist()

    bars = plt.bar(
        bar_positions,
        df['total_energy_mJ'],
        color=bar_colors,
        edgecolor='black'
    )

    # Value labels
    for idx, val in enumerate(df['total_energy_mJ']):
        if pd.notna(val):
            plt.text(idx, val + 0.001, f'{val:.3f}', ha='center', va='bottom', fontsize=9)

    # X-axis ticks
    plt.xticks(bar_positions, [str(int(sub)) for sub in df['Subarray']], rotation=45)
    plt.xlabel("Number of Subarrays Per Bank")

    # Legend
    legend_elements = [
        Patch(facecolor=color_palette[sub], edgecolor='black', label=f'{int(sub)}')
        for sub in subarray_vals
    ]
    plt.legend(handles=legend_elements, title='Number of Subarrays Per Bank',
               loc='center left', bbox_to_anchor=(1.02, 0.5), borderaxespad=0.)

    # Labels and layout
    plt.ylabel("Total Energy (mJ)")
    plt.title("Total Energy under Auto Refresh with Different Number of Subarrays per Bank")
    plt.grid(True, linestyle='--', alpha=0.5)
    plt.tight_layout(rect=[0, 0, 0.85, 1])  # Room for legend
    plt.show()

if __name__ == "__main__":
    json_file = 'subarray_analysis_trace_summary.json'
    df = load_trace_summary(json_file)
    plot_total_energy_auto_refresh(df)
