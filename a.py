import json
payload = {'1': {'byte_num': 10152, 'last_min_max_delay_record': 1686383148, 'loss_rate': 536870912, 'max_delay': 129, 'min_delay': 57, 'packet_num': 47, 'sum_delay': 3490}}

for key in payload:
    flows_msg[key] = payload[key]
