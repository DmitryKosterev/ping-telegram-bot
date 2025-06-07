import matplotlib.pyplot as plt
import pandas as pd
import sys
import os

if len(sys.argv) < 3:
    print("Usage: python generate_graph.py <IP>")
    sys.exit(1)

chatId = sys.argv[1]
ip = sys.argv[2]
filename = f'report_{chatId}_{ip}.csv'

# Проверка наличия файла
if not os.path.exists(filename):
    print(f"Файл {filename} не найден.")
    sys.exit(1)

# Загрузка данных
df = pd.read_csv(filename)

# Проверим, что нужные колонки есть
required_columns = {'date', 'time', 'status'}
if not required_columns.issubset(df.columns):
    print(f"Файл {filename} не содержит нужных колонок.")
    sys.exit(1)

# Создание временной метки
df['timestamp'] = pd.to_datetime(df['date'] + ' ' + df['time'])

# Кодирование статуса
df['reachable'] = df['status'].apply(
    lambda x: 2 if x in ['Connection restored', 'Connected'] else (1 if x == 'High latency' else 0)
)

# Построение графика
plt.figure(figsize=(12, 4))
plt.plot(df['timestamp'], df['reachable'], drawstyle='steps-post', marker='o')
plt.title(f'Доступность IP: {ip}')
plt.xlabel('Время')
plt.ylabel('Статус')
plt.grid(True)
plt.ylim(-0.1, 2.1)

plt.tight_layout()
plt.savefig(f'graph_{chatId}_{ip}.png')