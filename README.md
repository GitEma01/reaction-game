Traffic Forecasting App
An interactive web application built with Streamlit to predict hourly traffic volume (vehicles/hour) using an XGBoost machine learning model.

The application allows users to simulate traffic conditions by providing custom inputs such as date, time, weather conditions, and temperature. It supports both validation on historical data and future projections using a rolling forecast logic.

Key Features
Two Prediction Modes:

Historical Mode (2012-2018): Extracts actual lags (past data) from the dataset, generates the prediction, and compares it with the Ground Truth, calculating the percentage error. Useful for testing model accuracy.

Future Mode (>2018): Uses an iterative approach (Rolling Forecast) to predict traffic hour by hour into the future, feeding past predictions as new inputs (lags) for subsequent hours.

Dynamic Feature Engineering: The code calculates temporal variables (cyclic encoding with sine/cosine), lag variables (1h, 24h, 168h), rolling statistics (rolling mean/std), and weather variables on the fly.

Interactive Visualization: Uses Plotly to visually compare the point prediction with the typical average traffic pattern (weekdays vs. weekends).

Traffic Level Indicator: Intuitively classifies traffic into 4 levels (Low, Medium, High, Very High).

Assumed Project Structure
For this script to work correctly, the project must follow this folder hierarchy (as defined by the Path calls in the code):

Plaintext
your-project/
 |-- models/
 |   |-- xgboost_model_final.pkl      # Trained XGBoost model
 |-- data/
 |   |-- traffic_processed.csv        # Processed historical dataset
 |   |-- feature_columns.json         # JSON with the exact list of features
 |-- app/ (or main folder)
 |   |-- pages/
 |       |-- 3_previsioni.py          # This Streamlit script
 |-- README.md
Installation and Setup
Clone the repository:

Bash
git clone https://github.com/your-username/your-repo.git
cd your-repo
Create a virtual environment (recommended):

Bash
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
Install dependencies:
Make sure you have the following libraries installed. You can use a requirements.txt file or install them manually:

Bash
pip install streamlit pandas numpy plotly joblib xgboost scikit-learn
Run the Streamlit app:

Bash
streamlit run app/pages/3_previsioni.py
(Note: if you are using Streamlit's multipage navigation system, run the main app.py file).

Model Details (Machine Learning)
Algorithm: XGBoost Regressor

Target: traffic_volume (vehicles/hour)

Training Data: Historical data from 2012 to 2018.

Considered Inputs:

Temporal: Hour, day of the week, month, holidays, rush hours.

Weather: Temperature, weather type (Clear, Rain, Snow, etc.), precipitation.

Historical (Lags): Traffic values from the previous 1h, 2h, 3h, 24h, and 168h.

Known Limitations
In Future Mode, predicting dates very far ahead (months or years beyond the dataset) causes a drop in accuracy, as the rolling forecast error accumulates iteration after iteration.

The app assumes the user is testing an already trained model. If the files in /models or /data are missing, the app will display a warning and stop.
