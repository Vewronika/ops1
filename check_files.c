

int files_differ(const char *file1_path, const char *file2_path) {
    FILE *fp1 = fopen(file1_path, "rb");
    FILE *fp2 = fopen(file2_path, "rb");
    int ch1, ch2;

    if (fp1 == NULL || fp2 == NULL) {
        perror("Error opening file");
        if (fp1) fclose(fp1);
        if (fp2) fclose(fp2);
        return -1;
    }

    do {
        ch1 = fgetc(fp1);
        ch2 = fgetc(fp2);

        if (ch1 != ch2) {
            fclose(fp1);
            fclose(fp2);
            return 1;
        }
    } while (ch1 != EOF && ch2 != EOF);

    if (ch1 == EOF && ch2 == EOF) {
        fclose(fp1);
        fclose(fp2);
        return 0;
    } else {
        fclose(fp1);
        fclose(fp2);
        return 1;
    }
}
