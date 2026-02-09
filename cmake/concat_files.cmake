# Helper script to concatenate files.
# Usage: cmake -DOUTPUT=out -DINPUT0=a -DINPUT1=b ... -P concat_files.cmake

file (WRITE "${OUTPUT}" "")
set (i 0)
while (DEFINED INPUT${i})
  file (READ "${INPUT${i}}" content)
  file (APPEND "${OUTPUT}" "${content}")
  math (EXPR i "${i} + 1")
endwhile ()
