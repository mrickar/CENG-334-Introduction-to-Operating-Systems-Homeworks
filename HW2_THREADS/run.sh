rm -rf outputs
mkdir outputs
for i in  {1..50} 
do
    timeout 3s ./hw2 < "inputs/input$i.txt" > "outputs/output$i.txt"
done


