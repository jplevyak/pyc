# Regression: the pyc `string` stdlib shim (pyc_lib/string.py) with
# Python 3's canonical ascii_* character-class constants (issue 025
# bucket C: string.ascii_letters was the one missing member blocking
# the shedskin `solitaire` example).
import string
from string import ascii_letters, ascii_lowercase, ascii_uppercase

def main():
    print(string.ascii_letters)
    print(string.ascii_lowercase)
    print(string.ascii_uppercase)
    print(ascii_letters)
    print(ascii_lowercase)
    print(ascii_uppercase)
    print(string.digits)
    print(len(string.ascii_letters))
    print(ascii_letters[0], ascii_letters[26])

main()
