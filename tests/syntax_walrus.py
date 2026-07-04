def test_walrus():
    if (n := 5) > 3:
        print(n)
        
    while (m := n - 1) > 0:
        print(m)
        n = m
        
    a = [y := x * 2 for x in [1, 2, 3]]
    print(a)

test_walrus()
