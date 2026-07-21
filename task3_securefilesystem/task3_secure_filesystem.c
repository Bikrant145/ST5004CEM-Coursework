/*
 * ST5004CEM - Task 3: File System Operations and Security
 * task3_secure_filesystem.c
 *
 * Demonstrates:
 *   1. File create / read / write / delete
 *   2. User authentication (SHA-256 hashed passwords)
 *   3. Permission system (owner/group/others - read/write/execute)
 *   4. Encryption / decryption using AES-256-CBC (OpenSSL)
 *   5. Audit logging of every file operation
 *
 * Compile:  gcc task3_secure_filesystem.c -o task3_secure_filesystem -lcrypto
 * Run:      ./task3_secure_filesystem
 *
 * On first run, creates users.txt with three demo accounts:
 *   admin / admin123   (group: admin)
 *   alice / alice123   (group: staff)
 *   bob   / bob123     (group: guest)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define MAX_USERS 20
#define MAX_FILES 50
#define MAX_LINE 256
#define USERNAME_LEN 32
#define GROUP_LEN 32
#define FILENAME_LEN 128

typedef struct {
    char username[USERNAME_LEN];
    char password_hash[65];
    char group[GROUP_LEN];
} User;

typedef struct {
    char filename[FILENAME_LEN];
    char owner[USERNAME_LEN];
    char group[GROUP_LEN];
    int perm;
} FilePerm;

User users[MAX_USERS];
int num_users = 0;

FilePerm file_perms[MAX_FILES];
int num_file_perms = 0;

char current_user[USERNAME_LEN] = "";
char current_group[GROUP_LEN] = "";
int logged_in = 0;

void hash_password(const char *password, char *output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)password, strlen(password), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    output_hex[64] = '\0';
}

void audit_log(const char *action, const char *filename, const char *result) {
    FILE *f = fopen("audit.log", "a");
    if (!f) return;
    time_t now = time(NULL);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(f, "[%s] user=%s action=%s file=%s result=%s\n",
            timebuf, logged_in ? current_user : "(none)", action, filename, result);
    fclose(f);
}

void load_users(void) {
    FILE *f = fopen("users.txt", "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && num_users < MAX_USERS) {
        User u;
        if (sscanf(line, "%31s %64s %31s", u.username, u.password_hash, u.group) == 3) {
            users[num_users++] = u;
        }
    }
    fclose(f);
}

void save_user(User u) {
    FILE *f = fopen("users.txt", "a");
    if (!f) return;
    fprintf(f, "%s %s %s\n", u.username, u.password_hash, u.group);
    fclose(f);
}

void seed_default_users(void) {
    FILE *check = fopen("users.txt", "r");
    if (check) {
        fclose(check);
        return;
    }
    struct { const char *name; const char *pass; const char *group; } defaults[] = {
        {"admin", "admin123", "admin"},
        {"alice", "alice123", "staff"},
        {"bob",   "bob123",   "guest"},
    };
    for (int i = 0; i < 3; i++) {
        User u;
        strncpy(u.username, defaults[i].name, USERNAME_LEN);
        strncpy(u.group, defaults[i].group, GROUP_LEN);
        hash_password(defaults[i].pass, u.password_hash);
        users[num_users++] = u;
        save_user(u);
    }
    printf("First run: created default accounts (admin/admin123, alice/alice123, bob/bob123)\n");
}

int authenticate(const char *username, const char *password) {
    char hash[65];
    hash_password(password, hash);
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password_hash, hash) == 0) {
            strncpy(current_user, username, USERNAME_LEN);
            strncpy(current_group, users[i].group, GROUP_LEN);
            return 1;
        }
    }
    return 0;
}

void load_file_perms(void) {
    FILE *f = fopen("file_perms.txt", "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && num_file_perms < MAX_FILES) {
        FilePerm fp;
        if (sscanf(line, "%127s %31s %31s %d", fp.filename, fp.owner, fp.group, &fp.perm) == 4) {
            file_perms[num_file_perms++] = fp;
        }
    }
    fclose(f);
}

void save_file_perms(void) {
    FILE *f = fopen("file_perms.txt", "w");
    if (!f) return;
    for (int i = 0; i < num_file_perms; i++) {
        fprintf(f, "%s %s %s %d\n",
                file_perms[i].filename, file_perms[i].owner,
                file_perms[i].group, file_perms[i].perm);
    }
    fclose(f);
}

FilePerm *find_file_perm(const char *filename) {
    for (int i = 0; i < num_file_perms; i++) {
        if (strcmp(file_perms[i].filename, filename) == 0)
            return &file_perms[i];
    }
    return NULL;
}

int has_permission(const char *filename, int bit) {
    FilePerm *fp = find_file_perm(filename);
    if (!fp) return 0;

    int applicable_digit;
    if (strcmp(fp->owner, current_user) == 0) {
        applicable_digit = (fp->perm / 100) % 10;
    } else if (strcmp(fp->group, current_group) == 0) {
        applicable_digit = (fp->perm / 10) % 10;
    } else {
        applicable_digit = fp->perm % 10;
    }
    return (applicable_digit & bit) != 0;
}

void create_file(void) {
    char filename[FILENAME_LEN];
    int perm;
    printf("Filename to create: ");
    scanf("%127s", filename);
    printf("Permission (3-digit octal, e.g. 640): ");
    scanf("%d", &perm);

    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Failed to create file.\n");
        audit_log("CREATE", filename, "FAILED");
        return;
    }
    fclose(f);

    FilePerm fp;
    strncpy(fp.filename, filename, FILENAME_LEN);
    strncpy(fp.owner, current_user, USERNAME_LEN);
    strncpy(fp.group, current_group, GROUP_LEN);
    fp.perm = perm;
    file_perms[num_file_perms++] = fp;
    save_file_perms();

    printf("File '%s' created. Owner=%s, Group=%s, Perm=%d\n",
           filename, current_user, current_group, perm);
    audit_log("CREATE", filename, "SUCCESS");
}

void write_file(void) {
    char filename[FILENAME_LEN];
    printf("Filename to write to: ");
    scanf("%127s", filename);

    if (!has_permission(filename, 2)) {
        printf("PERMISSION DENIED: you do not have write access to '%s'\n", filename);
        audit_log("WRITE", filename, "DENIED");
        return;
    }

    printf("Enter content (single line): ");
    char content[512];
    scanf(" %[^\n]", content);

    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Failed to open file for writing.\n");
        audit_log("WRITE", filename, "FAILED");
        return;
    }
    fprintf(f, "%s\n", content);
    fclose(f);
    printf("Written to '%s'.\n", filename);
    audit_log("WRITE", filename, "SUCCESS");
}

void read_file(void) {
    char filename[FILENAME_LEN];
    printf("Filename to read: ");
    scanf("%127s", filename);

    if (!has_permission(filename, 4)) {
        printf("PERMISSION DENIED: you do not have read access to '%s'\n", filename);
        audit_log("READ", filename, "DENIED");
        return;
    }

    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("File not found or cannot be opened.\n");
        audit_log("READ", filename, "FAILED");
        return;
    }
    char line[512];
    printf("--- Contents of %s ---\n", filename);
    while (fgets(line, sizeof(line), f))
        printf("%s", line);
    printf("--- End ---\n");
    fclose(f);
    audit_log("READ", filename, "SUCCESS");
}

void delete_file(void) {
    char filename[FILENAME_LEN];
    printf("Filename to delete: ");
    scanf("%127s", filename);

    if (!has_permission(filename, 2)) {
        printf("PERMISSION DENIED: you do not have write access needed to delete '%s'\n", filename);
        audit_log("DELETE", filename, "DENIED");
        return;
    }

    if (remove(filename) == 0) {
        printf("File '%s' deleted.\n", filename);
        for (int i = 0; i < num_file_perms; i++) {
            if (strcmp(file_perms[i].filename, filename) == 0) {
                for (int j = i; j < num_file_perms - 1; j++)
                    file_perms[j] = file_perms[j + 1];
                num_file_perms--;
                break;
            }
        }
        save_file_perms();
        audit_log("DELETE", filename, "SUCCESS");
    } else {
        printf("Failed to delete file (does it exist?).\n");
        audit_log("DELETE", filename, "FAILED");
    }
}

void change_permissions(void) {
    char filename[FILENAME_LEN];
    int new_perm;
    printf("Filename to change permissions on: ");
    scanf("%127s", filename);

    FilePerm *fp = find_file_perm(filename);
    if (!fp) {
        printf("No permission record found for this file.\n");
        return;
    }
    if (strcmp(fp->owner, current_user) != 0) {
        printf("PERMISSION DENIED: only the owner (%s) can change permissions.\n", fp->owner);
        audit_log("CHMOD", filename, "DENIED");
        return;
    }
    printf("New permission (3-digit octal): ");
    scanf("%d", &new_perm);
    fp->perm = new_perm;
    save_file_perms();
    printf("Permissions for '%s' updated to %d.\n", filename, new_perm);
    audit_log("CHMOD", filename, "SUCCESS");
}

void derive_key(const char *passphrase, unsigned char *key_out) {
    SHA256((const unsigned char *)passphrase, strlen(passphrase), key_out);
}

void encrypt_file(void) {
    char filename[FILENAME_LEN];
    char passphrase[128];
    printf("Filename to encrypt: ");
    scanf("%127s", filename);

    if (!has_permission(filename, 4) || !has_permission(filename, 2)) {
        printf("PERMISSION DENIED: need read+write access to encrypt '%s'\n", filename);
        audit_log("ENCRYPT", filename, "DENIED");
        return;
    }

    printf("Enter encryption passphrase: ");
    scanf("%127s", passphrase);

    FILE *in = fopen(filename, "rb");
    if (!in) {
        printf("Cannot open file.\n");
        audit_log("ENCRYPT", filename, "FAILED");
        return;
    }
    fseek(in, 0, SEEK_END);
    long insize = ftell(in);
    fseek(in, 0, SEEK_SET);
    unsigned char *plaintext = malloc(insize);
    fread(plaintext, 1, insize, in);
    fclose(in);

    unsigned char key[32];
    derive_key(passphrase, key);
    unsigned char iv[16];
    RAND_bytes(iv, sizeof(iv));

    unsigned char *ciphertext = malloc(insize + EVP_MAX_BLOCK_LENGTH);
    int len, ciphertext_len;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
    EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, insize);
    ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    char outname[FILENAME_LEN + 5];
    snprintf(outname, sizeof(outname), "%s.enc", filename);
    FILE *out = fopen(outname, "wb");
    fwrite(iv, 1, sizeof(iv), out);
    fwrite(ciphertext, 1, ciphertext_len, out);
    fclose(out);

    free(plaintext);
    free(ciphertext);

    printf("File encrypted -> %s (AES-256-CBC)\n", outname);
    audit_log("ENCRYPT", filename, "SUCCESS");
}

void decrypt_file(void) {
    char filename[FILENAME_LEN];
    char passphrase[128];
    printf("Encrypted filename (e.g. secret.txt.enc): ");
    scanf("%127s", filename);

    printf("Enter decryption passphrase: ");
    scanf("%127s", passphrase);

    FILE *in = fopen(filename, "rb");
    if (!in) {
        printf("Cannot open encrypted file.\n");
        audit_log("DECRYPT", filename, "FAILED");
        return;
    }
    unsigned char iv[16];
    fread(iv, 1, sizeof(iv), in);

    fseek(in, 0, SEEK_END);
    long total = ftell(in);
    long ciphertext_len = total - sizeof(iv);
    fseek(in, sizeof(iv), SEEK_SET);

    unsigned char *ciphertext = malloc(ciphertext_len);
    fread(ciphertext, 1, ciphertext_len, in);
    fclose(in);

    unsigned char key[32];
    derive_key(passphrase, key);

    unsigned char *plaintext = malloc(ciphertext_len + EVP_MAX_BLOCK_LENGTH);
    int len, plaintext_len;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
    EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len);
    plaintext_len = len;
    int ok = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    if (ok != 1) {
        printf("DECRYPTION FAILED: wrong passphrase or corrupted file.\n");
        audit_log("DECRYPT", filename, "FAILED - wrong passphrase");
        free(ciphertext);
        free(plaintext);
        return;
    }

    char outname[FILENAME_LEN + 10];
    snprintf(outname, sizeof(outname), "%s.decrypted", filename);
    FILE *out = fopen(outname, "wb");
    fwrite(plaintext, 1, plaintext_len, out);
    fclose(out);

    free(ciphertext);
    free(plaintext);

    printf("File decrypted -> %s\n", outname);
    audit_log("DECRYPT", filename, "SUCCESS");
}

void view_audit_log(void) {
    FILE *f = fopen("audit.log", "r");
    if (!f) {
        printf("No audit log yet.\n");
        return;
    }
    char line[MAX_LINE];
    printf("--- Audit Log ---\n");
    while (fgets(line, sizeof(line), f))
        printf("%s", line);
    fclose(f);
}

int login_prompt(void) {
    char username[USERNAME_LEN], password[128];
    printf("Username: ");
    scanf("%31s", username);
    printf("Password: ");
    scanf("%127s", password);

    if (authenticate(username, password)) {
        logged_in = 1;
        printf("Login successful. Welcome, %s (group: %s)\n", current_user, current_group);
        audit_log("LOGIN", "-", "SUCCESS");
        return 1;
    } else {
        printf("Login failed: invalid username or password.\n");
        audit_log("LOGIN", "-", "FAILED");
        return 0;
    }
}

void print_menu(void) {
    printf("\n=== Secure File Manager ===\n");
    printf("Logged in as: %s\n", logged_in ? current_user : "(not logged in)");
    printf("1. Login\n");
    printf("2. Create file\n");
    printf("3. Read file\n");
    printf("4. Write file\n");
    printf("5. Delete file\n");
    printf("6. Change file permissions\n");
    printf("7. Encrypt file\n");
    printf("8. Decrypt file\n");
    printf("9. View audit log\n");
    printf("0. Exit\n");
    printf("Choice: ");
}

int main(void) {
    seed_default_users();
    load_users();
    load_file_perms();

    int choice;
    int running = 1;

    while (running) {
        print_menu();
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }

        switch (choice) {
            case 1: login_prompt(); break;
            case 2:
                if (!logged_in) { printf("Please log in first.\n"); break; }
                create_file(); break;
            case 3:
                if (!logged_in) { printf("Please log in first.\n"); break; }
                read_file(); break;
            case 4:
                if (!logged_in) { printf("Please log in first.\n"); break; }
                write_file(); break;
            case 5:
                if (!logged_in) { printf("Please log in first.\n"); break; }
                delete_file(); break;
            case 6:
                if (!logged_in) { printf("Please log in first.\n"); break; }
                change_permissions(); break;
            case 7:
                if (!logged_in) { printf("Please log in first.\n"); break; }
                encrypt_file(); break;
            case 8:
                if (!logged_in) { printf("Please log in first.\n"); break; }
                decrypt_file(); break;
            case 9:
                if (!logged_in) { printf("Please log in first.\n"); break; }
                view_audit_log(); break;
            case 0:
                running = 0;
                break;
            default:
                printf("Invalid choice.\n");
        }
    }

    printf("Goodbye.\n");
    return 0;
}
