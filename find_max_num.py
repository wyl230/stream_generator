# 打开文件，每一行读入并转换为数字，忽略非数字字符
with open('output.txt', 'r') as f:
    numbers = []
    l = f.readlines()
    for line in l:
        a = line.split(',')
        for i in a:
            if i.isdigit():
                numbers.append(int(i))

# 求出数字列表中的最大值
# print
print(len(numbers))
max_number = max(numbers)

print(max_number)

