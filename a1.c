#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>


typedef struct {
    char name[18];
    int16_t type;
    int size;
} section;

int checkFileFormat(const char* path, int print);
int getNrOfSections(const char* path);
int getLineOffsetOfSector(const char* path, int section_nr);
int getSectSize(const char* path, int section_nr);

int getStringLength(const char* text){
    int length = 0;
    while(text[length] != '\0') length++;
    return length; 
}

int isPath(const char* text){
    if(getStringLength(text) <= 5){
    	return 1;
    }
    char prefix[6];
    for(int i = 0 ; i < 5; i++) prefix[i] = text[i];
    prefix[5] = '\0';	
    
    if(strcmp(prefix, "path=")){
    	return 1;
    }	
    return 0;
}

char* getPath(const char* text){
      char* path = (char*)malloc(getStringLength(text) * sizeof(char));
      int j = 0;
      for(int i = 5; i < getStringLength(text); i++){
          path[j] = text[i];
      	  j++;
      }
      path[j] = '\0';
      return path;
}

int checkPathExists(const char* path){
    struct stat st;
    if (stat(path, &st) == 0) {
	return S_ISDIR(st.st_mode); // Check if it's a directory
    } else {
	return 1; // Path does not exist
    }
}

int isFilteringOption(const char* text){
    char option[getStringLength(text) + 1];
    int length = getStringLength(text);
    
    int i = 0 ;
    for(i = 0; i < length; i++){
    	option[i] = text[i];
    }
    option[i] = '\0';
    
    if(length >= 17){
    	for(int i = 0 ; i < 17; i++){
    		option[i] = text[i];
    	}
    	option[17] = '\0';
    	if(strcmp(option, "name_starts_with=") == 0){
    		return 1;
    	}
    }
    if(length >= 14){
    	for(int i = 0 ; i < 14 ;i++){
    		option[i] = text[i];
    	}
    	option[14] = '\0';
    	if(strcmp(option, "has_perm_write") == 0){
    		return 2;
    	}
    }
    return -1;
}

char* getFilteringOption(const char* text, int option){
    int length = getStringLength(text);
    char* filterOption = (char*)malloc((length + 1) * sizeof(char));
	
    int j = 0 ;
    if(option == 1){
        for(int i = 17; i < length; i++){
            filterOption[j] = text[i];
	    j++;
        }
    filterOption[j] = '\0';
    }else if(option == 2){
        for(int i = 0; i < length; i++){
	    filterOption[j] = text[i];
	    j++;
	}
	filterOption[j] = '\0';
	}
	else{
	    return NULL;
	}
	return filterOption; 
}



int checkFindAllCondition(const char* path){
    int nr_of_sections = getNrOfSections(path);
    if(nr_of_sections < 3) return -1;
	
    int fd = open(path, O_RDONLY);
    if(fd == -1) return -1;
	
    int nr_of_sections_with_15_lines = 0;
    for(int i = 1 ; i <= nr_of_sections ;i++){
        lseek(fd, 27 * i , SEEK_SET);
	int start_offset = 0;
	ssize_t bytesRead = read(fd, &start_offset, 4);
		
	if(bytesRead != 4){
	    close(fd);
	    return -1;
	}
	
	lseek(fd, (27*i) + 4, SEEK_SET);
	int sect_size = 0;
	bytesRead = read(fd, &sect_size, 4);
	if(bytesRead != 4){
	    close(fd);
	    return -1;
	}
	
		
	int line_count = 1;
	lseek(fd, start_offset, SEEK_SET);
	char sectString[sect_size + 1];
	bytesRead = read(fd, &sectString, sect_size);
		
	if(bytesRead != sect_size){
	    close(fd);
	    return -1;
	}
	sectString[sect_size] = '\0';
	for(int i = 0; i < sect_size ; i++){
	    if(i+1 < sect_size && sectString[i] == 0x0d && sectString[i+1] == 0x0a)
	        line_count++;
	}
	if(line_count == 15) nr_of_sections_with_15_lines++;
    }	
    if(nr_of_sections_with_15_lines >= 3){
        close(fd);
	return 0;
    }
    close(fd);
    return -1;
}

void list_folders_recursively(const char* path, const char* filteringOption){
    DIR* dir;
    struct dirent* entry;
    struct stat st;

    if ((dir = opendir(path)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            char entry_path[1024];
            if (path[strlen(path) - 1] == '/') {
                snprintf(entry_path, sizeof(entry_path), "%s%s", path, entry->d_name);
            }
            else{
                snprintf(entry_path, sizeof(entry_path), "%s/%s", path, entry->d_name);
            }
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                if (filteringOption == NULL) {
                    printf("%s/%s\n", path, entry->d_name);
                } else if (strcmp(filteringOption, "has_perm_write") == 0) {
                    struct stat st;
                    if(stat(entry_path, &st) == -1){
                    	continue;
                    }
                    if( (st.st_mode & S_IWUSR) == S_IWUSR){
                    	printf("%s/%s\n", path, entry->d_name);
                    }
                    
                }
                else if(strcmp(filteringOption, "findall") == 0){
                    if(stat(entry_path, &st) == 0 && S_ISREG(st.st_mode)){
               	        if(checkFileFormat(entry_path, 0) == 0){
                	    if(checkFindAllCondition(entry_path) == 0){
                	        printf("%s\n", entry_path);	
                            }
                        }
                    }   
                }
                else{
                    int filterLength = getStringLength(filteringOption);
                    int ok = 1;

                    if (getStringLength(entry->d_name) >= filterLength) {
                        for (int i = 0; i < filterLength; i++) {
                            if (filteringOption[i] != entry->d_name[i])
                                ok = 0;
                        }
                        if (ok == 1)
                            printf("%s/%s\n", path, entry->d_name);
                    }
                }
	    if (stat(entry_path, &st) == 0 && S_ISDIR(st.st_mode)) {
		if(filteringOption == NULL){
		    list_folders_recursively(entry_path, filteringOption);
		}
		else if(filteringOption != NULL && strcmp(filteringOption, "has_perm_write") != 0){
		    list_folders_recursively(entry_path, filteringOption);
		}
		else if(strcmp(filteringOption, "findall") == 0){
		    list_folders_recursively(entry_path, filteringOption);
		}
		else{
		    if(access(entry_path, W_OK) == 0){
		        list_folders_recursively(entry_path, filteringOption);
		    }	
		}	
            }		
        }
    }
    closedir(dir);
    }
}


void list_folders(const char* path, int recursive, const char* filteringOption){
    DIR* dir;
    struct dirent* entry;
    
    if(checkPathExists(path)) printf("SUCCESS\n");
    else{
    	printf("ERROR\n");
    	printf("Path doesn't exist\n");
    	return;
    }
    
    if(recursive == 1) {
    	list_folders_recursively(path, filteringOption);
    	return;
    }
    
    if ((dir = opendir(path)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            char entry_path[1024];
            snprintf(entry_path, sizeof(entry_path), "%s/%s", path, entry->d_name);
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                if(filteringOption == NULL){
                    printf("%s/%s\n", path, entry->d_name);
                }
                else if(strcmp(filteringOption, "has_perm_write") == 0){
                    struct stat st;
                    if(stat(entry_path, &st) == -1){
                    	continue;
                    }
                    if( (st.st_mode & S_IWUSR) == S_IWUSR){
                    	printf("%s/%s\n", path, entry->d_name);
                    }
                }
                else{
                    int filterLength = getStringLength(filteringOption);
                    int ok = 1;
                    if(getStringLength(entry->d_name) >= filterLength){
                        for(int i = 0 ; i < filterLength; i++){
                            if(filteringOption[i] != entry->d_name[i]) ok = 0;
                        }
                    if(ok == 1) printf("%s/%s\n", path, entry->d_name);
                    }
                }        
            }
        }
        closedir(dir);
    }
}

int getNrOfSections(const char* path){
	int fd = open(path, O_RDONLY);
	
	lseek(fd, 7, SEEK_SET);
	char nr_of_sections; 
	read(fd, &nr_of_sections, 1);
	
	close(fd);
	return (int)(nr_of_sections);
}

int checkFileFormat(const char* path, int print){
    int fd = open(path, O_RDONLY);
    
    struct stat st;
    long int file_size = 0;
    if(stat(path, &st) == 0){
    	file_size = st.st_size;
    }
    else{
    	close(fd);
    	return -1;
    }
    if(file_size < 9){
    	close(fd);
    	return -1;
    }
    
    lseek(fd, 7, SEEK_SET);
    char nr_of_sections;
    read(fd, &nr_of_sections, 1);
    
    lseek(fd, 6, SEEK_SET);
    unsigned char version;
    read(fd, &version, 1);
    
    lseek(fd, 4, SEEK_SET);
    int16_t header_size;
    read(fd, &header_size, 2);
    
    lseek(fd, 0, SEEK_SET);
    char pime[5];
    read(fd, &pime, 4);
    pime[4] = '\0';
    if(strcmp(pime, "Pime") == 0){
    	
    }else{
    	close(fd);
    	if(print == 1){
    		printf("ERROR\n");
    		printf("wrong magic\n");
    	}
    	return -1;
    }
    
    if(version < 109 || version > 152){
    	close(fd);
    	if(print == 1){
    		printf("ERROR\n");
    		printf("wrong version\n");
    	}
    	return -1;
    }
    if(nr_of_sections != 2 && (nr_of_sections < 7 || nr_of_sections > 18)){
    	close(fd);
   	if(print == 1){
   		printf("ERROR\n");
    		printf("wrong sect_nr\n");
   	}
    	return -1;
    }
    
    int section_headers = nr_of_sections * 27 + 8;
    
    if(file_size < section_headers){
    	close(fd);
    	return -1;
    }
    
    int current_section_header_offset = section_headers-1;
    section sec_array[nr_of_sections];
    
    for(int i = 0 ; i < nr_of_sections ; i++){
    	lseek(fd, current_section_header_offset, SEEK_SET);
    	
    	int current_sect_size;
    	current_section_header_offset -= 3;
    	lseek(fd, current_section_header_offset, SEEK_SET);
    	read(fd, &current_sect_size, 4);
    	
    	int current_sect_offset;
    	current_section_header_offset -= 4;
    	lseek(fd, current_section_header_offset, SEEK_SET);
    	read(fd, &current_sect_offset, 4);
    	
    	int16_t current_sect_type;
    	current_section_header_offset -= 2;
    	lseek(fd, current_section_header_offset, SEEK_SET);
    	read(fd, &current_sect_type, 2);
    	
    	if(current_sect_type != 63 && current_sect_type != 61 && current_sect_type != 19 && current_sect_type != 90){
            close(fd);
    	    if(print == 1){
    	        printf("ERROR\n");
    	 	printf("wrong sect_types\n");
    	    }
    	    return -1;
    	}
    	
    	char current_sect_name[18];
    	current_section_header_offset -= 17;
    	lseek(fd, current_section_header_offset, SEEK_SET);
    	read(fd, &current_sect_name, 17);
    	current_sect_name[17] = '\0';
    	
    	
    	section current_sec;
    	strcpy(current_sec.name, current_sect_name);
    	current_sec.type = current_sect_type;
    	current_sec.size = current_sect_size;
    	
    	sec_array[i] = current_sec;
    	
    	current_section_header_offset -= 1;
    }
    
    if(print == 1){
    	printf("SUCCESS\n");
        printf("version=%d\n", version);
        printf("nr_sections=%d\n", nr_of_sections);
    
        for(int i = nr_of_sections - 1 ; i >= 0; i--){
    	    printf("section%d: %s %d %d\n", nr_of_sections - i, sec_array[i].name,     sec_array[i].type, sec_array[i].size);
        }
    }
    close(fd);
    return 0;
}

int parseIntFromString(const char* text){
    int result = 0;
    int sign = 1;
	
    if(*text == '-'){
        sign = -1;
	text++;
    }
    while(*text != '\0'){
	result = result * 10 + (*text - '0');
	text++;
    }
    return sign * result; 
}

int getSectionNr(const char* text){

    int length = getStringLength(text);
    if(length < 8) return 0;
    char secNr[length-7];

    int i = 8;
    for(i = 8 ; i < length ; i++){
	secNr[i-8] = text[i];
    }
    secNr[i-8] = '\0';

    int res = parseIntFromString(secNr);
    return res;
}

int isSectionNr(const char* text){
    if(getStringLength(text) < 9){
	return -1;
    }
    char sec[getStringLength(text)];
    for(int i = 0 ; i < 9 ; i++){
	sec[i] = text[i];
    }
    sec[8] = '\0';
    if(strcmp(sec, "section=") == 0){
	return 0;
    }
    return -1;
}

int getLineNr(const char* text){
    char* lineNr = (char*)malloc((getStringLength(text) - 4) * sizeof(char));
    int i = 5;
    for(i = 5 ; i < getStringLength(text); i++){
	lineNr[i-5] = text[i];
    }
    lineNr[i-5] = '\0';
    int res = parseIntFromString(lineNr);
    free(lineNr);
	
    return res;
}

int isLineNr(const char* text){
    if(getStringLength(text) < 6){
	return -1;
    }
    char line[6];
    for(int i = 0 ; i < 5; i++){
	line[i] = text[i];
    } 
    line[5] = '\0';
    if(strcmp(line, "line=") == 0) return 0;
    return 1;
}

int fileExists(const char* path){
    if(access(path, F_OK) != -1){
	return 1;
    }
    return 0;
}

int getLineOffsetOfSector(const char* path, int section_nr){
    int fd = open(path, O_RDONLY);
    int sector_line_offset = 0;
	
    lseek(fd, 27 * section_nr, SEEK_SET);
    read(fd, &sector_line_offset, 4);
	
    close(fd);
    return sector_line_offset;
}

int getSectSize(const char* path, int section_nr){
    int fd = open(path, O_RDONLY);
    int sector_size = 0;

    lseek(fd, (27 * section_nr) + 4, SEEK_SET);
    read(fd, &sector_size, 4);
	
    close(fd);
    return sector_size;
}

void printLineFromSection(const char* path, int line_start_offset, int line_nr, int sect_size) {
    int fd = open(path, O_RDONLY);

    if (fd == -1) {
        printf("ERROR: Can't open file descriptor\n");
        close(fd);
        return;
    }

    char* sectString = (char*)malloc((sect_size + 1) * sizeof(char));

    if (sectString == NULL) {
        close(fd); 
        return;
    }

    lseek(fd, line_start_offset, SEEK_SET);

    ssize_t bytes_read = read(fd, sectString, sect_size); 

    if (bytes_read < 0) {
        printf("ERROR: Read error\n");
        close(fd);
        free(sectString); 
        return;
    }

    sectString[bytes_read] = '\0'; 

    int current_line = 1;
    int start_of_line = -1;
    int lineCharCount = 0;

    for (int i = 0; i < bytes_read; i++) {
        if (i + 1 < bytes_read && sectString[i] == 0x0d && sectString[i + 1] == 0x0a) {
            current_line++;
        } else {
            if (current_line == line_nr) {
                if (start_of_line == -1) {
                    start_of_line = i;
                }
                lineCharCount++;
            }
        }
    }

    if (current_line < line_nr) {
        close(fd);
        free(sectString); 
        return;
    }

    printf("SUCCESS\n");
    for (int i = start_of_line + lineCharCount; i >= start_of_line; i--) {
    	if(sectString[i] != 0x00)
        	printf("%c", sectString[i]);
    }

    close(fd);
    free(sectString);
}


int main(int argc, char **argv)
{	
    if(argc >= 2) {
        if(strcmp(argv[1], "variant") == 0) {
            printf("84876\n");
        }
     	else if(strcmp(argv[1], "list") == 0){
      	    if(argc < 3){
      	    	return -1;
      	    	
      	    }else if(argc >= 3 && argc <= 5){
      	    	int recursive = 0;
      	    	char* path = NULL;
      	    	char* filteringOption = NULL;
      	    	
      	    	int foundPath = 0;
      	    	int foundRecursive = 0;
      	    	int foundFilter = 0;
      	    	int foundCount = 0 ;
      	    	
      	    	
      	    	for(int i = 2; i < argc; i++){
      	    	    if(foundCount == 3) break;
      	    	    
      	    	    if(isPath(argv[i]) == 0 && foundPath == 0){
      	    	    	foundPath = 1;
      	    	    	path = getPath(argv[i]);
      	    	    	foundCount++;
      	    	    }
      	    	    if(strcmp(argv[i], "recursive") == 0 && foundRecursive == 0){
      	    	    	foundRecursive = 1;
      	    	    	recursive = 1;
      	    	    	foundCount++;
      	    	    }
      	    	    if(isFilteringOption(argv[i]) == 1 && foundFilter == 0){
      	    	    	foundFilter = 1;
      	    	    	filteringOption = getFilteringOption(argv[i] , 1);
      	    	    	foundCount++;
      	    	    }else if(isFilteringOption(argv[i]) == 2 && foundFilter == 0){
      	    	    	foundFilter = 1;
      	    	    	filteringOption = getFilteringOption(argv[i], 2);
      	    	    	foundCount++;
      	    	    }
      	    	}
      	    	if(foundCount >= 1 && foundPath == 1){
      	    	    list_folders(path, recursive, filteringOption);
      	    	}
      	    	free(path);
      	    	free(filteringOption);
      	    }
     	}
     	else if(strcmp(argv[1], "parse") == 0){
     	    if(argc < 3){
                return -1;
     	    }
     	    char* path = NULL;
     	    if(isPath(argv[2]) == 0){
     		path = getPath(argv[2]);
     		checkFileFormat(path, 1);
     			
     		if(path != NULL) free(path);
     	    }
     	    else return -1;
     			
     		
     	}  
     	else if(strcmp(argv[1], "extract") == 0){
     	    if(argc < 5){
     	        return -1;
     	    }
     	    char* path = NULL;
     	    int section_nr = 0;
     	    int line_nr = 0;
     		
     	    int foundPath = 0;
     	    int foundSectionNr = 0;
     	    int foundLineNr = 0;
     		
     	    for(int i = 2; i < argc; i++){
     			
     	        if(isSectionNr(argv[i]) == 0 && foundSectionNr == 0){
     		    foundSectionNr = 1;
     		    section_nr = getSectionNr(argv[i]);
     		}
     		if(isPath(argv[i]) == 0 && foundPath == 0){
     		    foundPath = 1;
     		    path = getPath(argv[i]);
     	        }
     		if(isLineNr(argv[i]) == 0 && foundLineNr == 0){
     		    foundLineNr = 1;
     		    line_nr = getLineNr(argv[i]);
     		}
     			
     	    }
     	    if(fileExists(path) == 0){
     	        printf("ERROR\n");
     		printf("invalid file\n");
     		if(path != NULL) free(path);
     		return -1;
     	    }
     	    if(section_nr > getNrOfSections(path)){
     		printf("ERROR\n");
     		printf("invalid section\n");
     		if(path != NULL) free(path);
     		return -1;
     	    }
     		
     	    int start_offset = getLineOffsetOfSector(path, section_nr);
     	    int sect_size = getSectSize(path, section_nr);
     	    printLineFromSection(path, start_offset, line_nr, sect_size);
     	    if(path != NULL) free(path);
     	}
     	else if(strcmp(argv[1], "findall") == 0){
     	    if(argc < 3) return -1;
     	    char* path = NULL;
     		
     	    if(isPath(argv[2]) == 0){
     		path = getPath(argv[2]);
     		printf("SUCCESS\n");
     		list_folders_recursively(path, "findall");
     	    }
     	    if(path != NULL) free(path);
     	}  
    }
    return 0;
}
