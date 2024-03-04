import sys

#* 更改cfg中执行的命令

def modify_line_after_match(filename, target, new_content):
    with open(filename, 'r') as file:
        lines = file.readlines()

    #* 查找目标内容并修改它下一行的内容
    for i, line in enumerate(lines):
        if target in line:
            if i + 1 < len(lines):
                lines[i + 1] = new_content + '\n'
                break
            else:
                print("Target is at the last line, no line to modify after it.")
                return

    #* 将修改后的内容写回文件
    with open(filename, 'w') as file:
        file.writelines(lines)


def main(argv):
    file_name = argv[0]
    app_name = argv[1]
    graph_name = argv[2]
    specific_line_content = 'process0 = {'
    new_content = '    command = "' + app_name + ' ' + graph_name + '";'
    modify_line_after_match(file_name, specific_line_content, new_content)

if __name__ == '__main__':
    main(sys.argv[1:])