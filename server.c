// Server code

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080

struct product
{
    int id;
    char name[50];
    int qty;
    float cost;
};
sem_t product_semaphores[100]; // declare array of semaphores

// Define an array of products
struct product products[100];

int num_products = 0;

void generate_log_file()
{
    // Get current time
    time_t current_time = time(NULL);
    char *timestamp = asctime(localtime(&current_time));
    timestamp[strlen(timestamp) - 1] = '\0'; // Remove newline character

    // Open file for writing
    int fd = open("admin_log.txt", O_CREAT | O_WRONLY, 0644);
    if (fd == -1)
    {
        perror("Error opening file");
        return;
    }

    // Write header information to file
    char header[100];
    sprintf(header, "Stock log created on %s\n\n", timestamp);
    write(fd, header, strlen(header));

    // Write product details to file
    char line[500];
    for (int i = 0; i < num_products; i++)
    {
        sprintf(line, "Product ID: %d, Name: %s, Cost: Rs. %.2f, Quantity: %d\n",
                products[i].id, products[i].name, products[i].cost, products[i].qty);
        write(fd, line, strlen(line));
    }

    // Close file
    close(fd);
}

void update_product()
{
    int choice, quantity;
    float price;

    // Display the list of products
    if (num_products == 0)
    {
        printf("No products available\n");
    }
    printf("\nAvailable Products:\n");
    for (int i = 0; i < num_products; i++)
    {
        printf("%d. %s - Rs. %2f - Quantity: %d\n", products[i].id, products[i].name, products[i].cost, products[i].qty);
    }

    // Read the product ID from the user
    printf("Enter the product ID you want to update: ");
    scanf("%d", &choice);

    // Check if the product ID is invalid
    if (choice < 1 || choice > num_products)
    {
        printf("Invalid product ID. Please try again.\n");
        return;
    }

    // Read the new quantity from the user
    printf("Enter the new quantity: ");
    scanf("%d", &quantity);

    // Check if the quantity is valid
    if (quantity < 0)
    {
        printf("Invalid quantity. Please try again.\n");
        return;
    }

    // Read the new price from the user
    printf("Enter the new price: ");
    scanf("%f", &price);

    // Check if the price is valid
    if (price < 0.0)
    {
        printf("Invalid price. Please try again.\n");
        return;
    }

    // Update the product quantity and price
    products[choice - 1].qty = quantity;
    products[choice - 1].cost = price;

    printf("Product updated successfully.\n");
}

void add_product()
{
    // Prompt the user for the product information
    printf("\nEnter the product name: ");
    scanf("%s", products[num_products].name);

    printf("Enter the product cost: ");
    scanf("%f", &products[num_products].cost);

    printf("Enter the product quantity: ");
    scanf("%d", &products[num_products].qty);

    // Assign a unique ID to the product
    products[num_products].id = num_products + 1;

    // Apply a semaphore lock on the added item
    sem_init(&product_semaphores[num_products], 0, 1);
    sem_wait(&product_semaphores[num_products]);

    // Print a message after locking the item
    printf("Product added to cart and locked.\n\n");

    // Increment the number of products
    (num_products)++;
}

void delete_product()
{
    int product_id;
    printf("Enter product id: ");
    scanf("%d", &product_id);
    // Find the product with the given ID
    int index = -1;
    for (int i = 0; i < num_products; i++)
    {
        if (products[i].id == product_id)
        {
            index = i;
            break;
        }
    }

    // If the product was not found, print an error message and return
    if (index == -1)
    {
        printf("Product with ID %d not found.\n", product_id);
        return;
    }

    // Shift the products to the left to remove the deleted product
    for (int i = index; i < num_products - 1; i++)
    {
        products[i] = products[i + 1];
    }

    // Decrement the number of products
    (num_products)--;

    printf("Product with ID %d deleted.\n", product_id);
}

void modify_cart(int client_socket)
{
    int qty, status = 0;
    struct product p;
    read(client_socket, &qty, sizeof(qty));
    read(client_socket, &p, sizeof(struct product));
    for (int i = 0; i < num_products; i++)
    {
        if(products[i].id == p.id)
        {
            if(p.qty <= products[i].qty)
                status = 1;
        }
    }
    write(client_socket, &status, sizeof(status));
}

void add_to_cart(int client_socket)
{
    int product_id, quantity;
    // send the number of products to the client
    write(client_socket, &num_products, sizeof(num_products));

    // send the array of products to the client
    write(client_socket, products, sizeof(struct product) * num_products);
}

void display_products(int client_socket)
{
    // send the number of products to the client
    write(client_socket, &num_products, sizeof(num_products));

    for(int i = 0;i<num_products; i++)
    {
        write(client_socket, &products[i], sizeof(struct product));
    }
}

void go_to_payment_gateway(int client_socket)
{
    int num_items_in_cart;
    int status = 1;
    read(client_socket, &num_items_in_cart, sizeof(num_items_in_cart));
    for (int i = 0; i < num_items_in_cart; i++)
    {
        struct product p;
        read(client_socket, &p, sizeof(p));
        for (int j = 0; j < num_products; j++)
        {
            if (products[j].id == p.id)
            {
                // remove the previous lock
                sem_post(&product_semaphores[j]);

                // apply new lock to update the quantity
                sem_wait(&product_semaphores[i]);

                // Maintain the edge case of concurrently another user buying
                // the same product or admin updating the quantity
                if (products[j].qty >= p.qty)
                {
                    products[j].qty -= p.qty;
                    printf("New qty : %d\n", products[j].qty);
                }
                else
                {
                    printf("Sufficient quantity not available\n");
                    status = 0;
                    write(client_socket, &status, sizeof(status));
                    return ;
                }
            }
        }
    }
    write(client_socket, &status, sizeof(status));
}

int main(int argc, char const *argv[])
{
    int server_fd, valread, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char menu[40] = "Menu:\n1. Admin\n2. User\n";

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Server running....\n");

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    printf("Connection accepted\n");

    int flag = 1;
    while (flag)
    {
        // Send the menu to the client
        write(new_socket, menu, sizeof(menu));
        // Wait for the client's choice
        int uchoice;
        read(new_socket, &uchoice, sizeof(uchoice));

        if (uchoice == 1)
        {
            while (1)
            {
                int num;
                printf("1. Add a new Product\n2. Delete a product\n3. Update price/quantity of a product\n4. Exit\n");
                scanf("%d", &num);

                if (num == 1)
                {
                    add_product();
                }
                else if (num == 2)
                {
                    delete_product();
                }
                else if (num == 3)
                {
                    update_product();
                }
                else if (num == 4)
                {
                    generate_log_file();
                    break;
                }
                else
                {
                    printf("Invalid choice\n");
                }
            }
        }
        else if (uchoice == 2)
        {
            while (1)
            {
                int choice;
                // read user choice to what to do out of 5 available options
                int temp = read(new_socket, &choice, sizeof(choice));
                if (temp > 0)
                {
                    if (choice == 1)
                    {
                        display_products(new_socket);
                    }
                    else if (choice == 3)
                    {
                        add_to_cart(new_socket);
                    }
                    else if (choice == 4)
                    {
                        modify_cart(new_socket);
                    }
                    else if (choice == 5)
                    {
                        go_to_payment_gateway(new_socket);
                        flag = 0;
                        break;
                    }
                }
            }
        }
        else
        {
            printf("Invalid choice\n");
        }
    }

    close(new_socket);
    // return NULL;
}