for k in 24 25 26 27 28 29 
do
	for j in 14 16 18 20 22
	do
		./create_format "deneme" $k & pid1=$!
		wait $pid1
		./test "deneme" $j &>> output.txt & pid2=$!
		wait $pid2
	done
done