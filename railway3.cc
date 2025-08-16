#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <limits>
#include <random>
#include <memory>
#include <chrono>
#include <sstream>
#include <algorithm>

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

    // Executes non-query SQL (INSERT, UPDATE, DELETE, CREATE)
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

    // Executes a SELECT query and returns the results
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

    // Transaction management
    bool beginTransaction() { return executeUpdate("BEGIN IMMEDIATE TRANSACTION;"); }
    bool commit() { return executeUpdate("COMMIT;"); }
    bool rollback() { return executeUpdate("ROLLBACK;"); }

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

        executeUpdate(
            "CREATE TABLE IF NOT EXISTS trains ("
            "train_number TEXT PRIMARY KEY NOT NULL,"
            "train_name TEXT NOT NULL,"
            "source TEXT NOT NULL,"
            "destination TEXT NOT NULL,"
            "departure_time TEXT NOT NULL,"
            "journey_duration TEXT NOT NULL,"
            "total_ac_seats INTEGER NOT NULL,"
            "total_sleeper_seats INTEGER NOT NULL,"
            "ac_fare REAL NOT NULL,"
            "sleeper_fare REAL NOT NULL);"
        );

        executeUpdate(
            "CREATE TABLE IF NOT EXISTS schedules ("
            "schedule_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "train_number TEXT NOT NULL,"
            "departure_date TEXT NOT NULL,"
            "ac_seats_available INTEGER NOT NULL,"
            "sleeper_seats_available INTEGER NOT NULL,"
            "FOREIGN KEY(train_number) REFERENCES trains(train_number),"
            "UNIQUE(train_number, departure_date));" // Prevent duplicate schedules
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

    // ============================ FORMATTING FIX START ============================
    void displayAsHeader() const {
        const int W_NUM = 10, W_NAME = 45, W_SRC = 25, W_DEST = 25, W_DEP = 11, W_DUR = 10;
        std::cout << std::string(W_NUM + W_NAME + W_SRC + W_DEST + W_DEP + W_DUR + 19, '-') << std::endl;
        std::cout << "| " << std::left << std::setw(W_NUM) << "Train No."
                  << "| " << std::setw(W_NAME) << "Train Name"
                  << "| " << std::setw(W_SRC) << "Source"
                  << "| " << std::setw(W_DEST) << "Destination"
                  << "| " << std::setw(W_DEP) << "Departure"
                  << "| " << std::setw(W_DUR) << "Duration" << " |" << std::endl;
        std::cout << std::string(W_NUM + W_NAME + W_SRC + W_DEST + W_DEP + W_DUR + 19, '-') << std::endl;
    }

    void displayAsRow() const {
        const int W_NUM = 10, W_NAME = 45, W_SRC = 25, W_DEST = 25, W_DEP = 11, W_DUR = 10;

        auto truncate = [](const std::string& str, int width) {
            if (str.length() > width) {
                return str.substr(0, width - 1) + "."; // Truncate and add a '.'
            }
            return str;
        };

        std::cout << "| " << std::left << std::setw(W_NUM) << truncate(number, W_NUM)
                  << "| " << std::setw(W_NAME) << truncate(name, W_NAME)
                  << "| " << std::setw(W_SRC) << truncate(source, W_SRC)
                  << "| " << std::setw(W_DEST) << truncate(destination, W_DEST)
                  << "| " << std::setw(W_DEP) << truncate(departureTime, W_DEP)
                  << "| " << std::setw(W_DUR) << truncate(journeyDuration, W_DUR) << " |" << std::endl;
    }
    // ============================ FORMATTING FIX END ============================
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
            std::cout << "   Railway Reservation System\n";
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
                case 3: viewAllTrains(true); break; // true to pause
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
        std::cout << "Enter username: "; 
        std::cin >> username;

        std::string checkSql = "SELECT 1 FROM users WHERE username='" + username + "';";
        if (!DatabaseManager::getInstance().executeQuery(checkSql).empty()) {
            std::cout << "Username already exists. Please choose a different one.\n";
            pressEnterToContinue();
            return;
        }

        std::cout << "Enter password: "; 
        std::cin >> password;
        std::string sql = "INSERT INTO users (username, password) VALUES ('" + username + "', '" + password + "');";
        if (DatabaseManager::getInstance().executeUpdate(sql)) {
            std::cout << "Signup successful! You can now log in.\n";
        } else {
            std::cout << "An unexpected error occurred during signup.\n";
        }
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
        
        int totalAcSeats, totalSleeperSeats;
        double acFare, sleeperFare;
        std::cout << "Enter Total AC Seats: "; std::cin >> totalAcSeats;
        std::cout << "Enter AC Fare: "; std::cin >> acFare;
        std::cout << "Enter Total Sleeper Seats: "; std::cin >> totalSleeperSeats;
        std::cout << "Enter Sleeper Fare: "; std::cin >> sleeperFare;

        std::string sql = "INSERT INTO trains VALUES ('" + t.number + "', '" + t.name + "', '" + t.source + "', '" + t.destination + "', '" + t.departureTime + "', '" + t.journeyDuration + "', " + std::to_string(totalAcSeats) + ", " + std::to_string(totalSleeperSeats) + ", " + std::to_string(acFare) + ", " + std::to_string(sleeperFare) + ");";
        
        if (DatabaseManager::getInstance().executeUpdate(sql)) std::cout << "Train route added successfully!\n";
        else std::cout << "Failed to add train route (Train Number might already exist).\n";
        pressEnterToContinue();
    }

    void scheduleTrain() {
        std::cout << "--- Schedule a Train for a Date ---\n";
        viewAllTrains(false); // false to not pause
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

    void viewAllTrains(bool pause) {
        std::cout << "--- List of All Train Routes ---\n";
        auto results = DatabaseManager::getInstance().executeQuery("SELECT * FROM trains;");
        if (results.empty()) {
            std::cout << "No train routes found.\n";
        } else {
            Train t_header;
            t_header.displayAsHeader();
            for (const auto& row : results) {
                Train t = {row[0], row[1], row[2], row[3], row[4], row[5]};
                t.displayAsRow();
            }
            const int W_NUM = 10, W_NAME = 45, W_SRC = 25, W_DEST = 25, W_DEP = 11, W_DUR = 10;
            std::cout << std::string(W_NUM + W_NAME + W_SRC + W_DEST + W_DEP + W_DUR + 19, '-') << std::endl;
        }
        if (pause) pressEnterToContinue();
    }

    void deleteTrain() {
        std::cout << "--- Delete Train Route ---\n";
        viewAllTrains(false);
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
             // ============================ FORMATTING FIX START ============================
             const int W_TID = 15, W_USER = 15, W_NAME = 30, W_DATE = 12, W_CLASS = 10, W_SEATS = 7, W_FARE = 12;
             std::cout << std::string(W_TID + W_USER + W_NAME + W_DATE + W_CLASS + W_SEATS + W_FARE + 22, '-') << std::endl;
             std::cout << "| " << std::left << std::setw(W_TID) << "Ticket ID" << "| " << std::setw(W_USER) << "Username" << "| " << std::setw(W_NAME) << "Train Name" << "| " << std::setw(W_DATE) << "Date" << "| " << std::setw(W_CLASS) << "Class" << "| " << std::setw(W_SEATS) << "Seats" << "| " << std::setw(W_FARE) << "Fare" << " |" << std::endl;
             std::cout << std::string(W_TID + W_USER + W_NAME + W_DATE + W_CLASS + W_SEATS + W_FARE + 22, '-') << std::endl;
             
             double totalRevenue = 0.0;
             auto truncate = [](const std::string& str, int width) {
                if (str.length() > width) return str.substr(0, width - 1) + ".";
                return str;
             };

             for (const auto& row : results) {
                totalRevenue += std::stod(row[6]);
                std::cout << "| " << std::left 
                          << std::setw(W_TID) << truncate(row[0], W_TID) 
                          << "| " << std::setw(W_USER) << truncate(row[1], W_USER) 
                          << "| " << std::setw(W_NAME) << truncate(row[2], W_NAME) 
                          << "| " << std::setw(W_DATE) << row[3] 
                          << "| " << std::setw(W_CLASS) << row[4] 
                          << "| " << std::setw(W_SEATS) << row[5] 
                          << "| " << std::setw(W_FARE) << std::fixed << std::setprecision(2) << std::stod(row[6]) << " |" << std::endl;
             }
             std::cout << std::string(W_TID + W_USER + W_NAME + W_DATE + W_CLASS + W_SEATS + W_FARE + 22, '-') << std::endl;
             // ============================ FORMATTING FIX END ============================
             std::cout << "\n--- Total Revenue: " << std::fixed << std::setprecision(2) << totalRevenue << " ---\n";
        }
        pressEnterToContinue();
    }

    // --- User Functionality ---
    void bookTicket() {
        std::cout << "--- Book a Ticket ---\n";
        
        std::string sql = "SELECT s.schedule_id, t.train_name, t.source, t.destination, s.departure_date, s.ac_seats_available, s.sleeper_seats_available, t.ac_fare, t.sleeper_fare, t.train_number FROM schedules s JOIN trains t ON s.train_number = t.train_number WHERE s.departure_date >= date('now');";
        auto results = DatabaseManager::getInstance().executeQuery(sql);

        if (results.empty()) {
            std::cout << "No trains are currently scheduled for booking.\n";
            pressEnterToContinue();
            return;
        }

        // ============================ FORMATTING FIX START ============================
        std::cout << "\n--- All Scheduled Journeys ---\n";
        const int W_ID = 5, W_NAME = 30, W_ROUTE = 30, W_DATE = 12, W_AC = 25, W_SL = 25;
        std::cout << std::string(W_ID + W_NAME + W_ROUTE + W_DATE + W_AC + W_SL + 19, '-') << std::endl;
        std::cout << "| " << std::left 
                  << std::setw(W_ID) << "ID" << "| " 
                  << std::setw(W_NAME) << "Train Name" << "| " 
                  << std::setw(W_ROUTE) << "Route" << "| " 
                  << std::setw(W_DATE) << "Date" << "| " 
                  << std::setw(W_AC) << "AC Seats (Fare)" << "| " 
                  << std::setw(W_SL) << "Sleeper Seats (Fare)" << " |" << std::endl;
        std::cout << std::string(W_ID + W_NAME + W_ROUTE + W_DATE + W_AC + W_SL + 19, '-') << std::endl;

        auto truncate = [](const std::string& str, int width) {
            if (str.length() > width) return str.substr(0, width - 1) + ".";
            return str;
        };

        for (const auto& row : results) {
            std::string route = row[2] + " -> " + row[3];
            std::stringstream ac_info, sleeper_info;
            ac_info << row[5] << " (Rs " << std::fixed << std::setprecision(2) << std::stod(row[7]) << ")";
            sleeper_info << row[6] << " (Rs " << std::fixed << std::setprecision(2) << std::stod(row[8]) << ")";

            std::cout << "| " << std::left 
                      << std::setw(W_ID) << row[0] 
                      << "| " << std::setw(W_NAME) << truncate(row[1], W_NAME) 
                      << "| " << std::setw(W_ROUTE) << truncate(route, W_ROUTE) 
                      << "| " << std::setw(W_DATE) << row[4]
                      << "| " << std::setw(W_AC) << ac_info.str() 
                      << "| " << std::setw(W_SL) << sleeper_info.str() << " |" << std::endl;
        }
        std::cout << std::string(W_ID + W_NAME + W_ROUTE + W_DATE + W_AC + W_SL + 19, '-') << std::endl;
        // ============================ FORMATTING FIX END ============================

        int scheduleId;
        std::cout << "\nEnter the Schedule ID of the journey you want to book: ";
        std::cin >> scheduleId;

        const std::vector<std::string>* selectedTrainData = nullptr;
        for(const auto& row : results) {
            if(std::stoi(row[0]) == scheduleId) {
                selectedTrainData = &row;
                break;
            }
        }
        
        if (!selectedTrainData) {
            std::cout << "Invalid ID.\n"; pressEnterToContinue(); return;
        }
        
        const auto& trainData = *selectedTrainData;
        int acSeatsAvail = std::stoi(trainData[5]);
        int sleeperSeatsAvail = std::stoi(trainData[6]);
        double acFare = std::stod(trainData[7]);
        double sleeperFare = std::stod(trainData[8]);

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
            std::cout << "Invalid number of seats or not enough seats available.\n"; pressEnterToContinue(); return;
        }

        double totalFare = numSeats * farePerSeat;
        std::string ticketId = generateTicketId();

        std::cout << "\n--- Booking Confirmation ---\n";
        std::cout << "Train: " << trainData[1] << " (" << trainData[9] << ")\n";
        std::cout << "Class: " << chosenClass << " | Seats: " << numSeats << "\n";
        std::cout << "Total Fare: " << std::fixed << std::setprecision(2) << totalFare << "\n";
        
        char confirm;
        std::cout << "Confirm booking? (y/n): ";
        std::cin >> confirm;

        if (confirm == 'y' || confirm == 'Y') {
            auto& db = DatabaseManager::getInstance();
            if (!db.beginTransaction()) {
                std::cout << "Booking failed: Could not start transaction.\n";
                pressEnterToContinue();
                return;
            }

            std::string checkSeatsSql = "SELECT " + seatColumn + " FROM schedules WHERE schedule_id=" + std::to_string(scheduleId) + ";";
            auto currentSeatsResult = db.executeQuery(checkSeatsSql);
            if (currentSeatsResult.empty() || std::stoi(currentSeatsResult[0][0]) < numSeats) {
                db.rollback();
                std::cout << "Booking failed: Seats were taken by another user.\n";
                pressEnterToContinue();
                return;
            }
            
            int currentAvailableSeats = std::stoi(currentSeatsResult[0][0]);

            std::string bookingSql = "INSERT INTO bookings (ticket_id, username, schedule_id, class, num_seats, total_fare) VALUES ('" + ticketId + "', '" + loggedInUsername + "', " + std::to_string(scheduleId) + ", '" + chosenClass + "', " + std::to_string(numSeats) + ", " + std::to_string(totalFare) + ");";
            std::string updateSql = "UPDATE schedules SET " + seatColumn + " = " + std::to_string(currentAvailableSeats - numSeats) + " WHERE schedule_id=" + std::to_string(scheduleId) + ";";

            if (db.executeUpdate(bookingSql) && db.executeUpdate(updateSql)) {
                db.commit();
                std::cout << "Booking successful! Your Ticket ID is " << ticketId << "\n";
            } else {
                db.rollback();
                std::cout << "Booking failed due to a database error.\n";
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
                std::cout << "\n========================================\n";
                std::cout << "  Ticket ID:      " << row[0] << "\n";
                std::cout << "----------------------------------------\n";
                std::cout << "  Train:          " << row[1] << "\n";
                std::cout << "  Route:          " << row[2] << " -> " << row[3] << "\n";
                std::cout << "  Departure:      " << row[4] << " at " << row[5] << "\n";
                std::cout << "  Arrival:        " << TimeUtil::calculateArrival(row[4], row[5], row[6]) << "\n";
                std::cout << "  Class:          " << row[7] << "\n";
                std::cout << "  Seats:          " << row[8] << "\n";
                std::cout << "  Total Fare:     Rs " << std::fixed << std::setprecision(2) << std::stod(row[9]) << "\n";
                std::cout << "========================================\n";
            }
        }
        pressEnterToContinue();
    }

    void cancelTicket() {
        std::cout << "--- Cancel a Ticket ---\n";
        std::string ticketId;
        std::cout << "Enter Ticket ID to cancel: ";
        std::cin >> ticketId;

        auto& db = DatabaseManager::getInstance();

        if (!db.beginTransaction()) {
            std::cout << "Cancellation failed: Could not start transaction.\n";
            pressEnterToContinue(); return;
        }

        std::string sql = "SELECT schedule_id, class, num_seats FROM bookings WHERE ticket_id='" + ticketId + "' AND username='" + loggedInUsername + "';";
        auto results = db.executeQuery(sql);

        if (results.empty()) {
            db.rollback();
            std::cout << "Invalid Ticket ID or you do not own this ticket.\n"; 
            pressEnterToContinue(); 
            return;
        }

        int scheduleId = std::stoi(results[0][0]);
        std::string seatClass = results[0][1];
        int numSeats = std::stoi(results[0][2]);
        std::string seatColumn = (seatClass == "AC") ? "ac_seats_available" : "sleeper_seats_available";

        std::string deleteSql = "DELETE FROM bookings WHERE ticket_id='" + ticketId + "';";
        std::string updateSql = "UPDATE schedules SET " + seatColumn + " = " + seatColumn + " + " + std::to_string(numSeats) + " WHERE schedule_id=" + std::to_string(scheduleId) + ";";

        if (db.executeUpdate(deleteSql) && db.executeUpdate(updateSql)) {
            db.commit();
            std::cout << "Ticket cancelled successfully!\n";
        } else {
            db.rollback();
            std::cout << "Cancellation failed due to a database error.\n";
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
