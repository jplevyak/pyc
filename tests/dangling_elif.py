def main():
    i = 0
    while i < 3:
        i += 1
        if i == 1:
            print("one")
        elif i == 2:
            if i > 0:
                print("two-inner")
        elif i == 3:
            print("three")
main()
