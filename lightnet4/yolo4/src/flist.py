import sys
import numpy as np
import matplotlib.pyplot as plt
sys.path.insert(0, "/home/liqiang/AI_not_smart3/lightnet4/yolo4/src")

def multiply(a):
    print "Will compute", a, "times", a
    c=a*a
    return c


def showHistogram(inString):
    
    xString="-8.00 -6.00 -6.00 -5.00 -3.00 -2.00 -2.00 -2.00 -2.00 -1.00 -1.00 -1.00 -1.00 -1.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 2.00 2.00 2.00 2.00 3.00 3.00 3.00 3.00 3.00 3.00 3.00 3.00 4.00 4.00 4.00 4.00 4.00 4.00 5.00 6.00 7.00 7.00 7.00 8.00 8.00 8.00 8.00 9.00 9.00 9.00 9.00 9.00 9.00 9.00 9.00 9.00 9.00 10.00 10.00 10.00 11.00 11.00 11.00 11.00 11.00 11.00 11.00 11.00 12.00 12.00 12.00 12.00 12.00 12.00 13.00 13.00 13.00 13.00 13.00 14.00 14.00 14.00 14.00 14.00 14.00 14.00 14.00 15.00 15.00 15.00 15.00 15.00 15.00 15.00 15.00 16.00 16.00 16.00 16.00 16.00 16.00 16.00 16.00 16.00 17.00 17.00 17.00 17.00 17.00 17.00 18.00 18.00 18.00 19.00 20.00 20.00 20.00 20.00 20.00 20.00 20.00 21.00 21.00 21.00 21.00 21.00 21.00 21.00 21.00 21.00 22.00 22.00 22.00 22.00 22.00 22.00 22.00 22.00 22.00 22.00 22.00 22.00 22.00 22.00 22.00 22.00 22.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 23.00 24.00 24.00 25.00"
	yString="-12.00 -11.00 -10.00 -10.00 -10.00 -9.00 -8.00 -8.00 -8.00 -8.00 -8.00 -8.00 -7.00 -7.00 -7.00 -7.00 -6.00 -6.00 -6.00 -6.00 -6.00 -6.00 -5.00 -5.00 -5.00 -5.00 -5.00 -4.00 -4.00 -4.00 -4.00 -4.00 -4.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 2.00 2.00 2.00 2.00 2.00 2.00 2.00 2.00 3.00 3.00 3.00 3.00 3.00 4.00 5.00 7.00"

    xString=inString.split(" ")
    
    listString=map(float, listString)
    result=[int(float(x)) for x in listString]
    print result

    plt.hist(result, bins=len(result))  # arguments are passed to np.histogram
    plt.title("Histogram with 'auto' bins")
    plt.show()
    
    return
    
showHistogram(xString)