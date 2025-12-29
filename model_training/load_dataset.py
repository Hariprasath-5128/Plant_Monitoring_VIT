from datasets import load_dataset
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(BASE_DIR, "model_training")  # folder containing class subfolders

dataset = load_dataset(
    "imagefolder",
    data_dir=DATA_DIR,
    split={"train": "train[:80%]", "test": "train[80:]"},
)

print(dataset)
print("Train columns:", dataset["train"].column_names)
print("Test columns:", dataset["test"].column_names)
print("Example row:", dataset["train"][0])
