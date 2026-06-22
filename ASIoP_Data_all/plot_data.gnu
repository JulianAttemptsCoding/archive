
set terminal png size 1200,800
set output 'voltage_analysis.png'
set title 'CERN Voltage Analysis with IQR'
set xlabel 'Voltage (kV)'
set ylabel 'Average Values'
set grid
set key top left

# Plot averages with lines
plot 'averaged_data.dat' using 1:2 with linespoints title 'Column C' lw 2 pt 7 ps 1.5, \
     'averaged_data.dat' using 1:3 with linespoints title 'Column D' lw 2 pt 9 ps 1.5, \
     'averaged_data.dat' using 1:4 with linespoints title 'Column E' lw 2 pt 11 ps 1.5, \
     'iqr_data.dat' using 1:2 with lines title 'Q1 Column C' lt 1 lw 1 dt 2, \
     'iqr_data.dat' using 1:3 with lines title 'Q3 Column C' lt 1 lw 1 dt 2, \
     'iqr_data.dat' using 1:4 with lines title 'Q1 Column D' lt 2 lw 1 dt 2, \
     'iqr_data.dat' using 1:5 with lines title 'Q3 Column D' lt 2 lw 1 dt 2, \
     'iqr_data.dat' using 1:6 with lines title 'Q1 Column E' lt 3 lw 1 dt 2, \
     'iqr_data.dat' using 1:7 with lines title 'Q3 Column E' lt 3 lw 1 dt 2
