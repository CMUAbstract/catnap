import fileinput

f_data = []
for line in fileinput.input("input_mic.txt"):
    data = line.split(",")

for d in data:
    f_data.append(float(d) / 100)

filt = [0.05] * 20

param = 20
golden = None
while True:
    data_convd = []
    j = 0
    while True:
        result = 0
        k = 0
        stride = 20 // param
        while k < 20:
            result += (f_data[j + k] * filt[k]) * stride
            k += stride
        data_convd.append(result)
        j += 20
        if j >= len(f_data):
            break
    print(data_convd)
    if golden == None:
        golden = data_convd + []
    param = param // 2

    # Calc error
    num = 0
    denom = 0
    for i in range(len(golden)):
        num += (golden[i] - data_convd[i])**2
        denom += golden[i]**2
    print("Error: " + str(num / denom))
    if param == 0:
        break
