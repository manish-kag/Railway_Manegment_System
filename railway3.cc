#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <limits>
#include <random>
#include <memory>
#include <chrono>
#include <sstream>
#include <algorithm> // Added for std::find_if, but will replace with loop for compatibility

// This header file must be in the same folder as your .cpp file.
#include "sqlite3.h"

// --- Class Forward Declarations ---
class Train;
class Booking;

// ===================================================================
//  DatabaseManager Class (Singleton)
//  Handles all interactions with the SQLite database.
// ===================================================================
class DatabaseManager {
public:
    static DatabaseManager& getInstance() {
        static DatabaseManager instance;
        return instance;
    }

    bool executeUpdate(const std::string& sql) {
        char* zErrMsg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << zErrMsg << std::endl;
            sqlite3_free(zErrMsg);
            return false;
        }
        return true;
    }

    std::vector<std::vector<std::string>> executeQuery(const std::string& sql) {
        std::vector<std::vector<std::string>> results;
        char* zErrMsg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), callback, &results, &zErrMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << zErrMsg << std::endl;
            sqlite3_free(zErrMsg);
        }
        return results;
    }

private:
    DatabaseManager() {
        int rc = sqlite3_open("railway_advanced_oop.db", &db);
        if (rc) {
            std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
            exit(1);
        }
        initializeSchema();
    }

    ~DatabaseManager() {
        sqlite3_close(db);
    }

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    void initializeSchema() {
        executeUpdate(
            "CREATE TABLE IF NOT EXISTS users ("
            "username TEXT PRIMARY KEY NOT NULL,"
            "password TEXT NOT NULL);"
        );

        // Stores static train information
        executeUpdate(
            "CREATE TABLE IF NOT EXISTS trains ("
            "train_number TEXT PRIMARY KEY NOT NULL,"
            "train_name TEXT NOT NULL,"
            "source TEXT NOT NULL,"
            "destination TEXT NOT NULL,"
            "departure_time TEXT NOT NULL," // "HH:MM"
            "journey_duration TEXT NOT NULL," // "HH:MM" format for total travel time
            "total_ac_seats INTEGER NOT NULL,"
            "total_sleeper_seats INTEGER NOT NULL,"
            "ac_fare REAL NOT NULL,"
            "sleeper_fare REAL NOT NULL);"
        );

        // Stores a specific run of a train on a specific date
        executeUpdate(
            "CREATE TABLE IF NOT EXISTS schedules ("
            "schedule_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "train_number TEXT NOT NULL,"
            "departure_date TEXT NOT NULL," // "YYYY-MM-DD"
            "ac_seats_available INTEGER NOT NULL,"
            "sleeper_seats_available INTEGER NOT NULL,"
            "FOREIGN KEY(train_number) REFERENCES trains(train_number));"
        );
        
        executeUpdate(
            "CREATE TABLE IF NOT EXISTS bookings ("
            "ticket_id TEXT PRIMARY KEY NOT NULL,"
            "username TEXT NOT NULL,"
            "schedule_id INTEGER NOT NULL,"
            "class TEXT NOT NULL,"
            "num_seats INTEGER NOT NULL,"
            "total_fare REAL NOT NULL,"
            "date_of_booking TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "FOREIGN KEY(schedule_id) REFERENCES schedules(schedule_id));"
        );

        if (executeQuery("SELECT * FROM users WHERE username='admin';").empty()) {
            executeUpdate("INSERT INTO users (username, password) VALUES ('admin', 'admin123');");
            std::cout << "Default admin user created: admin / admin123" << std::endl;
        }
    }

    static int callback(void* data, int argc, char** argv, char** azColName) {
        auto* rows = static_cast<std::vector<std::vector<std::string>>*>(data);
        std::vector<std::string> row;
        for (int i = 0; i < argc; i++) {
            row.push_back(argv[i] ? argv[i] : "NULL");
        }
        rows->push_back(row);
        return 0;
    }

    sqlite3* db;
};

// ===================================================================
//  Date/Time Utility Functions
// ===================================================================
namespace TimeUtil {
    // Calculates arrival date and time string based on departure and duration
    std::string calculateArrival(const std::string& departureDate, const std::string& departureTime, const std::string& duration) {
        std::tm start_tm = {};
        std::stringstream ss_date(departureDate + " " + departureTime);
        ss_date >> std::get_time(&start_tm, "%Y-%m-%d %H:%M");

        int duration_hours = 0, duration_minutes = 0;
        std::stringstream ss_duration(duration);
        char colon;
        ss_duration >> duration_hours >> colon >> duration_minutes;

        auto start_time_point = std::chrono::system_clock::from_time_t(std::mktime(&start_tm));
        auto end_time_point = start_time_point + std::chrono::hours(duration_hours) + std::chrono::minutes(duration_minutes);
        
        std::time_t end_time_t = std::chrono::system_clock::to_time_t(end_time_point);
        std::tm* end_tm = std::localtime(&end_time_t);
        
        std::stringstream result;
        result << std::put_time(end_tm, "%Y-%m-%d %H:%M");
        return result.str();
    }
}


// ===================================================================
//  Train Class
// ===================================================================
class Train {
public:
    std::string number, name, source, destination, departureTime, journeyDuration;
    int totalAcSeats, totalSleeperSeats;
    double acFare, sleeperFare;

    void displayAsHeader() const {
         std::cout << std::left 
                   << std::setw(12) << "Train No." 
                   << std::setw(25) << "Train Name"
                   << std::setw(20) << "Source" 
                   << std::setw(20) << "Destination"
                   << std::setw(15) << "Departure"
                   << std::setw(15) << "Duration"
                   << std::endl;
        std::cout << std::string(107, '-') << std::endl;
    }

    void displayAsRow() const {
        std::cout << std::left 
                  << std::setw(12) << number 
                  << std::setw(25) << name
                  << std::setw(20) << source 
                  << std::setw(20) << destination
                  << std::setw(15) << departureTime
                  << std::setw(15) << journeyDuration
                  << std::endl;
    }
};


// ===================================================================
//  RailwaySystem Class
// ===================================================================
class RailwaySystem {
public:
    void run() {
        mainMenu();
    }

private:
    std::string loggedInUsername;

    // --- Utility Methods ---
    void clearScreen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
    }

    void pressEnterToContinue() {
        std::cout << "\nPress Enter to continue...";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cin.get();
    }

    std::string generateTicketId() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(100000, 999999);
        return "TKT" + std::to_string(distrib(gen));
    }

    // --- Main Menus ---
    void mainMenu() {
        int choice;
        do {
            clearScreen();
            std::cout << "========================================\n";
            std::cout << "   Railway Reservation System (Advanced)\n";
            std::cout << "========================================\n";
            std::cout << "1. User Login\n";
            std::cout << "2. User Signup\n";
            std::cout << "3. Admin Login\n";
            std::cout << "4. Exit\n";
            std::cout << "Enter your choice: ";
            std::cin >> choice;

            switch (choice) {
                case 1: handleUserLogin(); break;
                case 2: handleUserSignup(); break;
                case 3: handleAdminLogin(); break;
                case 4: std::cout << "Exiting system. Goodbye!\n"; break;
                default: std::cout << "Invalid choice.\n"; pressEnterToContinue();
            }
        } while (choice != 4);
    }

    void adminMenu() {
        int choice;
        do {
            clearScreen();
            std::cout << "--- Admin Menu ---\n";
            std::cout << "1. Add New Train Route\n";
            std::cout << "2. Schedule a Train for a Date\n";
            std::cout << "3. View All Train Routes\n";
            std::cout << "4. Delete Train Route\n";
            std::cout << "5. View All Bookings\n";
            std::cout << "6. Logout\n";
            std::cout << "Enter your choice: ";
            std::cin >> choice;

            switch (choice) {
                case 1: addTrain(); break;
                case 2: scheduleTrain(); break;
                case 3: viewAllTrains(); break;
                case 4: deleteTrain(); break;
                case 5: viewAllBookingsAdmin(); break;
                case 6: std::cout << "Logging out...\n"; break;
                default: std::cout << "Invalid choice.\n"; pressEnterToContinue();
            }
        } while (choice != 6);
    }

    void userMenu() {
        int choice;
        do {
            clearScreen();
            std::cout << "--- Welcome, " << loggedInUsername << "! ---\n";
            std::cout << "1. Book Ticket\n";
            std::cout << "2. View My Bookings\n";
            std::cout << "3. Cancel Ticket\n";
            std::cout << "4. Logout\n";
            std::cout << "Enter your choice: ";
            std::cin >> choice;

            switch (choice) {
                case 1: bookTicket(); break;
                case 2: viewMyBookings(); break;
                case 3: cancelTicket(); break;
                case 4: std::cout << "Logging out...\n"; break;
                default: std::cout << "Invalid choice.\n"; pressEnterToContinue();
            }
        } while (choice != 4);
    }

    // --- Authentication Handlers ---
    void handleUserSignup() {
        std::string username, password;
        std::cout << "--- User Signup ---\n";
        std::cout << "Enter username: "; std::cin >> username;
        std::cout << "Enter password: "; std::cin >> password;
        std::string sql = "INSERT INTO users (username, password) VALUES ('" + username + "', '" + password + "');";
        if (DatabaseManager::getInstance().executeUpdate(sql)) std::cout << "Signup successful!\n";
        else std::cout << "Username already exists.\n";
        pressEnterToContinue();
    }

    void handleUserLogin() {
        std::string username, password;
        std::cout << "--- User Login ---\n";
        std::cout << "Enter username: "; std::cin >> username;
        std::cout << "Enter password: "; std::cin >> password;
        std::string sql = "SELECT * FROM users WHERE username='" + username + "' AND password='" + password + "';";
        if (!DatabaseManager::getInstance().executeQuery(sql).empty()) {
            std::cout << "Login successful!\n";
            loggedInUsername = username;
            pressEnterToContinue();
            userMenu();
        } else {
            std::cout << "Invalid credentials.\n";
            pressEnterToContinue();
        }
    }

    void handleAdminLogin() {
        std::string username, password;
        std::cout << "--- Admin Login ---\n";
        std::cout << "Enter admin username: "; std::cin >> username;
        std::cout << "Enter admin password: "; std::cin >> password;
        if (username == "admin" && password == "admin123") {
            std::cout << "Admin login successful!\n";
            loggedInUsername = "admin";
            pressEnterToContinue();
            adminMenu();
        } else {
            std::cout << "Invalid credentials.\n";
            pressEnterToContinue();
        }
    }
    
    // --- Admin Functionality ---
    void addTrain() {
        Train t;
        std::cout << "--- Add New Train Route ---\n";
        std::cout << "Enter Train Number: "; std::cin >> t.number;
        std::cout << "Enter Train Name: "; std::cin.ignore(); std::getline(std::cin, t.name);
        std::cout << "Enter Source: "; std::getline(std::cin, t.source);
        std::cout << "Enter Destination: "; std::getline(std::cin, t.destination);
        std::cout << "Enter Departure Time (HH:MM): "; std::cin >> t.departureTime;
        std::cout << "Enter Journey Duration (HH:MM): "; std::cin >> t.journeyDuration;
        std::cout << "Enter Total AC Seats: "; std::cin >> t.totalAcSeats;
        std::cout << "Enter AC Fare: "; std::cin >> t.acFare;
        std::cout << "Enter Total Sleeper Seats: "; std::cin >> t.totalSleeperSeats;
        std::cout << "Enter Sleeper Fare: "; std::cin >> t.sleeperFare;

        std::string sql = "INSERT INTO trains VALUES ('" + t.number + "', '" + t.name + "', '" + t.source + "', '" + t.destination + "', '" + t.departureTime + "', '" + t.journeyDuration + "', " + std::to_string(t.totalAcSeats) + ", " + std::to_string(t.totalSleeperSeats) + ", " + std::to_string(t.acFare) + ", " + std::to_string(t.sleeperFare) + ");";
        
        if (DatabaseManager::getInstance().executeUpdate(sql)) std::cout << "Train route added successfully!\n";
        else std::cout << "Failed to add train route.\n";
        pressEnterToContinue();
    }

    void scheduleTrain() {
        std::cout << "--- Schedule a Train for a Date ---\n";
        viewAllTrains();
        std::string trainNumber, date;
        std::cout << "\nEnter Train Number to schedule: ";
        std::cin >> trainNumber;
        std::cout << "Enter Departure Date (YYYY-MM-DD): ";
        std::cin >> date;

        auto trainResult = DatabaseManager::getInstance().executeQuery("SELECT total_ac_seats, total_sleeper_seats FROM trains WHERE train_number='" + trainNumber + "';");
        if (trainResult.empty()) {
            std::cout << "Train not found.\n";
            pressEnterToContinue();
            return;
        }

        std::string totalAcSeats = trainResult[0][0];
        std::string totalSleeperSeats = trainResult[0][1];

        std::string sql = "INSERT INTO schedules (train_number, departure_date, ac_seats_available, sleeper_seats_available) VALUES ('" + trainNumber + "', '" + date + "', " + totalAcSeats + ", " + totalSleeperSeats + ");";
        if (DatabaseManager::getInstance().executeUpdate(sql)) {
            std::cout << "Train scheduled successfully for " << date << ".\n";
        } else {
            std::cout << "Failed to schedule train. It might already be scheduled for this date.\n";
        }
        pressEnterToContinue();
    }

    void viewAllTrains() {
        std::cout << "--- List of All Train Routes ---\n";
        auto results = DatabaseManager::getInstance().executeQuery("SELECT * FROM trains;");
        if (results.empty()) {
            std::cout << "No train routes found.\n";
        } else {
            Train().displayAsHeader();
            for (const auto& row : results) {
                Train t = {row[0], row[1], row[2], row[3], row[4], row[5]};
                t.displayAsRow();
            }
        }
        pressEnterToContinue();
    }

    void deleteTrain() {
        std::cout << "--- Delete Train Route ---\n";
        viewAllTrains();
        std::string trainNumber;
        std::cout << "\nEnter Train Number to delete: ";
        std::cin >> trainNumber;
        std::string sql = "DELETE FROM trains WHERE train_number='" + trainNumber + "';";
        if (DatabaseManager::getInstance().executeUpdate(sql)) std::cout << "Train route deleted successfully.\n";
        else std::cout << "Failed to delete train route.\n";
        pressEnterToContinue();
    }

    void viewAllBookingsAdmin() {
        std::cout << "--- All User Bookings ---\n";
        std::string sql = "SELECT b.ticket_id, b.username, t.train_name, s.departure_date, b.class, b.num_seats, b.total_fare FROM bookings b JOIN schedules s ON b.schedule_id = s.schedule_id JOIN trains t ON s.train_number = t.train_number;";
        auto results = DatabaseManager::getInstance().executeQuery(sql);

        if (results.empty()) {
            std::cout << "No bookings found.\n";
        } else {
             std::cout << std::left << std::setw(15) << "Ticket ID" << std::setw(15) << "Username" << std::setw(25) << "Train Name" << std::setw(15) << "Date" << std::setw(10) << "Class" << std::setw(10) << "Seats" << std::setw(15) << "Fare" << std::endl;
             std::cout << std::string(105, '-') << std::endl;
             double totalRevenue = 0.0;
             for (const auto& row : results) {
                totalRevenue += std::stod(row[6]);
                std::cout << std::left << std::setw(15) << row[0] << std::setw(15) << row[1] << std::setw(25) << row[2] << std::setw(15) << row[3] << std::setw(10) << row[4] << std::setw(10) << row[5] << std::setw(15) << std::fixed << std::setprecision(2) << std::stod(row[6]) << std::endl;
             }
             std::cout << "\n--- Total Revenue: " << std::fixed << std::setprecision(2) << totalRevenue << " ---\n";
        }
        pressEnterToContinue();
    }

    // --- User Functionality ---
    void bookTicket() {
        std::cout << "--- Book a Ticket ---\n";
        
        std::string sql = "SELECT s.schedule_id, t.train_number, t.train_name, t.source, t.destination, s.departure_date, t.departure_time, t.journey_duration, s.ac_seats_available, s.sleeper_seats_available, t.ac_fare, t.sleeper_fare FROM schedules s JOIN trains t ON s.train_number = t.train_number WHERE s.departure_date >= date('now');";
        auto results = DatabaseManager::getInstance().executeQuery(sql);

        if (results.empty()) {
            std::cout << "No trains are currently scheduled for booking.\n";
            pressEnterToContinue();
            return;
        }

        std::cout << "\n--- All Scheduled Journeys ---\n";
        std::cout << std::left 
                  << std::setw(5) << "ID" 
                  << std::setw(25) << "Train Name" 
                  << std::setw(20) << "Route" 
                  << std::setw(15) << "Date" 
                  << std::setw(25) << "AC Seats (Fare)" 
                  << std::setw(25) << "Sleeper Seats (Fare)" << std::endl;
        std::cout << std::string(115, '-') << std::endl;
        for (const auto& row : results) {
            std::string route = row[3] + " -> " + row[4];
            std::stringstream ac_info, sleeper_info;
            ac_info << row[8] << " (Rs " << std::fixed << std::setprecision(2) << std::stod(row[10]) << ")";
            sleeper_info << row[9] << " (Rs " << std::fixed << std::setprecision(2) << std::stod(row[11]) << ")";

            std::cout << std::left 
                      << std::setw(5) << row[0] 
                      << std::setw(25) << row[2] 
                      << std::setw(20) << route 
                      << std::setw(15) << row[5] 
                      << std::setw(25) << ac_info.str() 
                      << std::setw(25) << sleeper_info.str() << std::endl;
        }

        int scheduleId;
        std::cout << "\nEnter the Schedule ID of the journey you want to book: ";
        std::cin >> scheduleId;

        auto it = results.end();
        for (auto temp_it = results.begin(); temp_it != results.end(); ++temp_it) {
            if (std::stoi((*temp_it)[0]) == scheduleId) {
                it = temp_it;
                break;
            }
        }
        
        if (it == results.end()) {
            std::cout << "Invalid ID.\n"; pressEnterToContinue(); return;
        }
        const auto& trainData = *it;
        int acSeatsAvail = std::stoi(trainData[8]);
        int sleeperSeatsAvail = std::stoi(trainData[9]);
        double acFare = std::stod(trainData[10]);
        double sleeperFare = std::stod(trainData[11]);

        std::cout << "\nSelect Class:\n1. AC (Fare: " << acFare << ")\n2. Sleeper (Fare: " << sleeperFare << ")\n";
        int choice;
        std::cin >> choice;

        std::string chosenClass, seatColumn;
        int availableSeats = 0;
        double farePerSeat = 0.0;
        if (choice == 1) {
            chosenClass = "AC"; availableSeats = acSeatsAvail; farePerSeat = acFare; seatColumn = "ac_seats_available";
        } else if (choice == 2) {
            chosenClass = "Sleeper"; availableSeats = sleeperSeatsAvail; farePerSeat = sleeperFare; seatColumn = "sleeper_seats_available";
        } else {
            std::cout << "Invalid choice.\n"; pressEnterToContinue(); return;
        }

        int numSeats;
        std::cout << "Enter number of seats: ";
        std::cin >> numSeats;

        if (numSeats <= 0 || numSeats > availableSeats) {
            std::cout << "Not enough seats.\n"; pressEnterToContinue(); return;
        }

        double totalFare = numSeats * farePerSeat;
        std::string ticketId = generateTicketId();

        std::cout << "\n--- Booking Confirmation ---\n";
        std::cout << "Train: " << trainData[2] << " (" << trainData[1] << ")\n";
        std::cout << "Class: " << chosenClass << " | Seats: " << numSeats << "\n";
        std::cout << "Total Fare: " << std::fixed << std::setprecision(2) << totalFare << "\n";
        
        char confirm;
        std::cout << "Confirm booking? (y/n): ";
        std::cin >> confirm;

        if (confirm == 'y' || confirm == 'Y') {
            std::string bookingSql = "INSERT INTO bookings (ticket_id, username, schedule_id, class, num_seats, total_fare) VALUES ('" + ticketId + "', '" + loggedInUsername + "', " + std::to_string(scheduleId) + ", '" + chosenClass + "', " + std::to_string(numSeats) + ", " + std::to_string(totalFare) + ");";
            std::string updateSql = "UPDATE schedules SET " + seatColumn + " = " + std::to_string(availableSeats - numSeats) + " WHERE schedule_id=" + std::to_string(scheduleId) + ";";

            if (DatabaseManager::getInstance().executeUpdate(bookingSql) && DatabaseManager::getInstance().executeUpdate(updateSql)) {
                std::cout << "Booking successful! Your Ticket ID is " << ticketId << "\n";
            } else {
                std::cout << "Booking failed.\n";
            }
        } else {
            std::cout << "Booking cancelled.\n";
        }
        pressEnterToContinue();
    }

    void viewMyBookings() {
        std::cout << "--- My Bookings ---\n";
        std::string sql = "SELECT b.ticket_id, t.train_name, t.source, t.destination, s.departure_date, t.departure_time, t.journey_duration, b.class, b.num_seats, b.total_fare FROM bookings b JOIN schedules s ON b.schedule_id = s.schedule_id JOIN trains t ON s.train_number = t.train_number WHERE b.username='" + loggedInUsername + "';";
        auto results = DatabaseManager::getInstance().executeQuery(sql);

        if (results.empty()) {
            std::cout << "You have no bookings.\n";
        } else {
            for (const auto& row : results) {
                std::cout << "\n----------------------------------------\n";
                std::cout << "Ticket ID: " << row[0] << "\n";
                std::cout << "Train: " << row[1] << "\n";
                std::cout << "Route: " << row[2] << " -> " << row[3] << "\n";
                std::cout << "Departure: " << row[4] << " at " << row[5] << "\n";
                std::cout << "Arrival: " << TimeUtil::calculateArrival(row[4], row[5], row[6]) << "\n";
                std::cout << "Class: " << row[7] << " | Seats: " << row[8] << "\n";
                std::cout << "Total Fare: " << std::fixed << std::setprecision(2) << std::stod(row[9]) << "\n";
                std::cout << "----------------------------------------\n";
            }
        }
        pressEnterToContinue();
    }

    void cancelTicket() {
        std::cout << "--- Cancel a Ticket ---\n";
        std::string ticketId;
        std::cout << "Enter Ticket ID to cancel: ";
        std::cin >> ticketId;

        std::string sql = "SELECT schedule_id, class, num_seats FROM bookings WHERE ticket_id='" + ticketId + "' AND username='" + loggedInUsername + "';";
        auto results = DatabaseManager::getInstance().executeQuery(sql);

        if (results.empty()) {
            std::cout << "Invalid Ticket ID.\n"; pressEnterToContinue(); return;
        }

        int scheduleId = std::stoi(results[0][0]);
        std::string seatClass = results[0][1];
        int numSeats = std::stoi(results[0][2]);
        std::string seatColumn = (seatClass == "AC") ? "ac_seats_available" : "sleeper_seats_available";

        std::string deleteSql = "DELETE FROM bookings WHERE ticket_id='" + ticketId + "';";
        std::string updateSql = "UPDATE schedules SET " + seatColumn + " = " + seatColumn + " + " + std::to_string(numSeats) + " WHERE schedule_id=" + std::to_string(scheduleId) + ";";

        if (DatabaseManager::getInstance().executeUpdate(deleteSql) && DatabaseManager::getInstance().executeUpdate(updateSql)) {
            std::cout << "Ticket cancelled successfully!\n";
        } else {
            std::cout << "Cancellation failed.\n";
        }
        pressEnterToContinue();
    }
};

// ===================================================================
//  Main Function
// ===================================================================
int main() {
    RailwaySystem app;
    app.run();
    return 0;
}

