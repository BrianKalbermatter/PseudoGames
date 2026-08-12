# Arrays Bidimensionales 5x5
#!/bin/bash
#2D -HeatMap(matriz plana) o sea grafico plano

declare -A mat
  filas=5; cols=5

  for ((i=0; i<filas; i++)); do
      for ((j=0; j<cols; j++)); do
          mat[$i,$j]=$((i * cols + j))
      done
  done
gnuplot << EOF
\$datos << EOD
$(for ((i=0; i<filas; i++)); do
     for ((j=0; j<cols; j++)); do
         echo -n "${mat[$i,$j]} "
     done
     echo
done)
EOD
set terminal png; set output "matriz2d.png"
set view map
plot '\$datos' matrix with image title "2D"
EOF
