# automaton for float
# 0: first status (digit or skip)
state 0
# space characters
 ' '  0  skip
 '\t'  0  skip
 '\n'  0  skip
 '\r'  0  skip
# digits
 '0'  1  
 '1'  1 
 '2'  1
 '3'  1
 '4'  1
 '5'  1
 '6'  1
 '7'  1
 '8'  1
 '9'  1
# minus sign
 '-'  1
# floating point
 '.'  2
# exp 
 'e'  3
 'E'  3

# 1: digits 
state 1
# digits 
 '0'  1
 '1'  1
 '2'  1
 '3'  1
 '4'  1
 '5'  1
 '6'  1
 '7'  1
 '8'  1
 '9'  1
# floating point
 '.'  2
# exp 
 'e'  3
 'E'  3

# 2: after floating point
state 2
# digits 
 '0'  2
 '1'  2
 '2'  2
 '3'  2
 '4'  2
 '5'  2
 '6'  2
 '7'  2
 '8'  2
 '9'  2
# exp
 'e'  3
 'E'  3

# 3: exponential part 1
state 3
# digits
 '0'  4
 '1'  4
 '2'  4
 '3'  4
 '4'  4
 '5'  4
 '6'  4
 '7'  4
 '8'  4
 '9'  4
# sign
 '+'  4
 '-'  4

# 4: exponential part 2
state 4
 '0'  4
 '1'  4
 '2'  4
 '3'  4
 '4'  4
 '5'  4
 '6'  4
 '7'  4
 '8'  4
 '9'  4
