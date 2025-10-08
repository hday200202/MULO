#pragma once
#include <string>
#include <random>
#include <cstdlib>
#include <fstream>
#include <thread>

class EmailService {
private:
    static std::string generateVerificationCode() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        return std::to_string(dis(gen));
    }

    // python script to send email
    static bool sendSMTPEmailSync(const std::string& toEmail, const std::string& verificationCode) {
        std::string script = 
R"(
import smtplib
import sys
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

def send_email(to_email, verification_code):
    try:
        smtp_server = "smtp.gmail.com"
        smtp_port = 587
        from_email = "muloteam@gmail.com"
        from_password = "jxsx fufe fino mmjb"  # App password from Google
        
        # Create message
        msg = MIMEMultipart()
        msg['From'] = from_email
        msg['To'] = to_email
        msg['Subject'] = "MULO Verification Code"
        
        body = f"""Your MULO verification code is: {verification_code}"""
        msg.attach(MIMEText(body, 'plain'))
        
        # Send email
        server = smtplib.SMTP(smtp_server, smtp_port)
        server.starttls()
        server.login(from_email, from_password)
        text = msg.as_string()
        server.sendmail(from_email, to_email, text)
        server.quit()
        
        return True
        
    except Exception:
        return False

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(1)
    
    to_email = sys.argv[1]
    verification_code = sys.argv[2]
    
    success = send_email(to_email, verification_code)
    sys.exit(0 if success else 1)
)";

        std::string scriptPath = "/tmp/mulo_send_email.py";
        std::ofstream scriptFile(scriptPath);
        if (!scriptFile.is_open()) {
            return false;
        }
        scriptFile << script;
        scriptFile.close();

        std::string command = "python3 " + scriptPath + " \"" + toEmail + "\" \"" + verificationCode + "\"";
        int result = std::system(command.c_str());

        std::remove(scriptPath.c_str());
        
        return (result == 0);
    }

public:
    static bool sendVerificationEmail(const std::string& email, std::string& verificationCode) {
        verificationCode = generateVerificationCode();
        
        std::thread emailThread([email, verificationCode]() {
            sendSMTPEmailSync(email, verificationCode);
        });
        
        emailThread.detach();        
        return true;
    }
};