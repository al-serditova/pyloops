import time
import numpy as np

def run_benchmark(func, *args, num_runs=10, warmup_runs=2, element_size_bytes=4, **kwargs):
    # Определяем размер входных данных для расчета пропускной способности
    # Предполагаем, что первый аргумент — это наш входной массив
    try:
        num_elements = len(args[0])
    except (IndexError, TypeError):
        num_elements = 1_000_000  # Дефолтное значение, если не определили
        
    total_bytes = num_elements * element_size_bytes
    total_mb = total_bytes / (1024 * 1024)

    # Холостой прогрев (warmup)
    for _ in range(warmup_runs):
        func(*args, **kwargs)

    # Основной цикл замеров
    times = []
    for _ in range(num_runs):
        start = time.perf_counter()
        func(*args, **kwargs)
        end = time.perf_counter()
        times.append(end - start)

    # Расчет статистических метрик
    times = np.array(times)
    min_time = np.min(times)
    median_time = np.median(times)
    mean_time = np.mean(times)
    std_dev = np.std(times) # Среднеквадратичное отклонение

    # Расчет пропускной способности для лучшего (минимального) времени
    throughput_mb_s = total_mb / min_time
    meps = (num_elements / min_time) / 1_000_000

    # Вывод результатов
    print(f"=== Результаты для: {func.__name__} ===")
    print(f"Размер данных:       {num_elements:,} элементов ({total_mb:.2f} MB)")
    print(f"Количество прогонов: {num_runs} (+ {warmup_runs} разогрев)")
    print("-" * 50)
    print(f"Минимум:             {min_time * 1000:.4f} мс")
    print(f"Медиана:             {median_time * 1000:.4f} мс")
    print(f"Среднее:             {mean_time * 1000:.4f} мс (± {std_dev * 1000:.4f} мс)")
    print("-" * 50)
    print(f"Производительность: {meps:.2f} млн эл/сек (Meps)")
    print(f"Пропускная способность: {throughput_mb_s:.2f} MB/s")
    print("=" * 50 + "\n")

    return {
        "min": min_time,
        "median": median_time,
        "mean": mean_time,
        "throughput_mb_s": throughput_mb_s,
        "meps": meps
    }