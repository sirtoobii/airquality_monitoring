def calcChecksum(uuid_str:str):
    """
    Implemention of the "Fletcher's Checksum". To fit the result into two
    HEX character we compute the modulo 16 of sum.
    """
    data_string = uuid_str.upper()
    sum_1 = 0
    sum_2 = 0
    for c in data_string:
        sum_1 = (sum_1 + ord(c)) % 255
        sum_2 = (sum_1 + sum_2) % 255
    sum_1 = sum_1 % 16
    sum_2 = sum_2 % 16
    return sum_1, sum_2
