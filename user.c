// Client code

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define PORT 8080

struct product
{
    int id;
    char name[50];
    int qty;
    float cost;
};
sem_t cart_semaphores[100]; // declare array of semaphores

struct product cart[100];
int num_items_in_cart = 0;

// Function to iterate over the cart array and generate a bill file
void generate_bill()
{
    int bill_file_fd = open("bill.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);

    if (bill_file_fd == -1)
    {
        perror("Error opening bill file");
        exit(EXIT_FAILURE);
    }

    // Write header
    char *header = "Product ID\tProduct Name\tQuantity\tPrice\tTotal\n";
    write(bill_file_fd, header, strlen(header));

    // Iterate over cart items and write to file
    float total = 0.0;
    for (int i = 0; i < num_items_in_cart; i++)
    {
        int product_id = cart[i].id;
        char *product_name = cart[i].name;
        int quantity = cart[i].qty;
        float price = cart[i].cost;
        float item_total = quantity * price;
        total += item_total;

        char item_str[200];
        sprintf(item_str, "%d\t\t\t%s\t\t\t\t%d\t\t\t%.2f\t%.2f\n", product_id, product_name, quantity, price, item_total);
        write(bill_file_fd, item_str, strlen(item_str));
    }

    // Write footer
    char footer[100];
    sprintf(footer, "\nTotal:\t\t\t\t\t%.2f\n", total);
    write(bill_file_fd, footer, strlen(footer));

    // Close file
    close(bill_file_fd);

    printf("Bill generated successfully.\n");
}

void modify_cart(int server_socket)
{
    int num_products, choice, index, quantity;

    // Display the list of cart items
    printf("Your Cart:\n");
    for (int i = 0; i < num_items_in_cart; i++)
    {
        printf("%d. %s - Rs. %2f (Qty: %d)\n", i + 1, cart[i].name, cart[i].cost, cart[i].qty);
    }

    // Prompt the user to choose an action
    printf("Enter the Sr. No of the item to modify/delete (type 0 to cancel): ");
    scanf("%d", &choice);

    // Check if the choice is invalid
    if (choice < 0 || choice > num_items_in_cart)
    {
        printf("Invalid choice. Please try again.\n");
        return;
    }

    // If the choice is 0, cancel the operation
    if (choice == 0)
    {
        return;
    }

    // Get the index of the selected item in the cart array
    index = choice - 1;

    // Prompt the user to choose an action
    printf("Do you want to (1) modify or (2) delete this item? ");
    scanf("%d", &choice);

    // Check if the choice is valid
    if (choice != 1 && choice != 2)
    {
        printf("Invalid choice. Please try again.\n");
        return;
    }

    // If the user wants to modify the item
    if (choice == 1)
    {
        // Prompt the user to enter the new quantity
        printf("Enter the new quantity: ");
        scanf("%d", &quantity);

        // Check if the quantity is valid
        if (quantity < 1)
        {
            printf("Invalid quantity. Please try again.\n");
            return;
        }

        // Check if the quantity is more than available
        write(server_socket, &quantity, sizeof(quantity));
        write(server_socket, &cart[index], sizeof(struct product));
        int status = 0;
        read(server_socket, &status, sizeof(status));

        if (!status)
        {
            printf("Insufficient quantity. Please try again.\n");
            return;
        }

        // Update the quantity of the selected item in the cart array
        cart[index].qty = quantity;

        // Print the response
        printf("Item updated successfully\n");
    }
    // If the user wants to delete the item
    else
    {
        // Remove the selected item from the cart array
        for (int i = index; i < num_items_in_cart - 1; i++)
        {
            cart[i] = cart[i + 1];
        }
        num_items_in_cart--;

        // Print the response
        printf("Item deleted from cart\n");
    }
}

void print_cart()
{
    if (num_items_in_cart == 0)
        printf("Cart Empty\n");
    for (int i = 0; i < num_items_in_cart; i++)
    {
        printf("%d %s %2f %d\n", cart[i].id, cart[i].name, cart[i].cost, cart[i].qty);
    }
    printf("\n");
}

void add_to_cart(int server_socket)
{
    int num_products, choice, quantity;
    struct product products[100];

    // Prompt the user to choose a product and quantity
    printf("Enter the product ID you want to add to cart: ");

    // Receive the number of products from the server
    read(server_socket, &num_products, sizeof(num_products));

    // Receive the array of products from the server
    read(server_socket, products, sizeof(struct product) * num_products);

    // Display the list of products
    printf("Available Products:\n");
    for (int i = 0; i < num_products; i++)
    {
        printf("%d. %s - Rs. %2f\n", products[i].id, products[i].name, products[i].cost);
    }

    // Read the product ID from the user
    scanf("%d", &choice);

    // Check if the product ID is valid
    if (choice < 1 || choice > num_products)
    {
        printf("Invalid product ID. Please try again.\n");
        return;
    }

    // Read the quantity from the user
    printf("Enter the quantity: ");
    scanf("%d", &quantity);

    // Check if the quantity is valid
    if (quantity < 1)
    {
        printf("Invalid quantity. Please try again.\n");
        return;
    }

    // Check if the product is already in the cart
    for (int i = 0; i < num_items_in_cart; i++)
    {
        if (cart[i].id == products[choice - 1].id)
        {
            printf("Product already in cart. Please try again.\n");
            return;
        }
    }

    // Check if there is enough quantity available
    if (products[choice - 1].qty < quantity)
    {
        printf("Insufficient quantity. Please try again.\n");
        return;
    }

    // find the product with the given id
    struct product *selected_product = NULL;
    for (int i = 0; i < num_products; i++)
    {
        if (products[i].id == choice)
        {
            selected_product = &products[i];
            break;
        }
    }

    // update the product quantity and send the response to the client
    char response[100];
    if (selected_product != NULL && selected_product->qty >= quantity)
    {
        selected_product->qty -= quantity;
        sprintf(response, "Product added to cart\n");
    }
    else
    {
        if (selected_product == NULL)
        {
            sprintf(response, "Product not found\n");
        }
        else
        {
            sprintf(response, "Insufficient quantity\n");
        }
    }

    // If the product was added to the cart, update the cart array
    if (strcmp(response, "Product added to cart\n") == 0)
    {
        // Add the product to the cart
        cart[num_items_in_cart].id = products[choice - 1].id;
        strcpy(cart[num_items_in_cart].name, products[choice - 1].name);
        cart[num_items_in_cart].cost = products[choice - 1].cost;
        cart[num_items_in_cart].qty = quantity;

        // Apply a semaphore lock on the added item
        sem_init(&cart_semaphores[num_items_in_cart], 0, 1);
        sem_wait(&cart_semaphores[num_items_in_cart]);

        // Update the number of items in the cart
        num_items_in_cart++;

        // Print a message after locking the item
        printf("Product added to cart and locked.\n\n");
    }
}

void display_products(int server_socket)
{
    int num_products;

    // receive the number of products from the server
    read(server_socket, &num_products, sizeof(num_products));

    // print the array of products
    printf("Available products:\n");
    printf("ID\tName\tPrice\n");
    for (int i = 0; i < num_products; i++)
    {
        struct product p;
        read(server_socket, &p, sizeof(struct product));
        printf("%d\t%s\t%2f\n", p.id, p.name, p.cost);
    }
    printf("\n");
}

void go_to_payment_gateway(int server_socket)
{

    int unlock[num_items_in_cart];
    int status = 0;
    write(server_socket, &num_items_in_cart, sizeof(num_items_in_cart));
    for (int i = 0; i < num_items_in_cart; i++)
    {
        struct product p = cart[i];
        write(server_socket, &p, sizeof(struct product));
    }
    read(server_socket, &status, sizeof(status));
    if(status)
    {

        system("clear"); // clear the terminal
        printf("Calculating total amount...\n");

        // Iterate over the cart and calculate the total amount
        float total_amount = 0;
        for (int i = 0; i < num_items_in_cart; i++)
        {
            total_amount += cart[i].cost * cart[i].qty;
        }

        // Ask the client to pay the total amount
        printf("Your total amount is %.2f. Please pay this amount.\n", total_amount);
        float payment;
        scanf("%f", &payment);

        // Calculate and return the change
        float change = payment - total_amount;
        if (change < 0)
        {
            printf("Insufficient payment. Please try again.\n");
        }
        else
        {
            printf("Thank you for your payment. Your change is %.2f.\n", change);
        }
    }
    else
    {
        printf("Quantity demanded not available\n");
        return;
    }

}

int main()
{
    int valread, sock;
    struct sockaddr_in serv_addr;
    char choice[1024] = {0};
    char buffer[100];

    // Create socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    printf("Connection successfull\n");

    int flag = 1;
    while (flag)
    {
        char menu[40];
        // Receive the menu from the server
        read(sock, menu, sizeof(menu));
        printf("%s\n", menu);

        // Get the user's choice
        printf("Enter your choice: ");
        int uchoice;
        scanf("%d", &uchoice);
        // Send the user's choice to the server
        write(sock, &uchoice, sizeof(uchoice));

        if (uchoice == 2)
        {
            // printf("%s\n", msg2);
            while (1)
            {
                char msg[200] = "1. Display all the products\n2. Display the Cart\n3. Choose a product and quantity to add into the cart\n4. Edit the cart \n5. Go to payment gateway";
                printf("%s\n", msg);

                int num;
                scanf("%d", &num);
                write(sock, &num, sizeof(num));
                if (num == 1)
                {
                    display_products(sock);
                }
                else if (num == 2)
                {
                    print_cart();
                }
                else if (num == 3)
                {
                    add_to_cart(sock);
                }
                else if (num == 4)
                {
                    modify_cart(sock);
                }
                else if (num == 5)
                {
                    go_to_payment_gateway(sock);
                    int status;
                    read(sock, &status, sizeof(status));
                    if (status)
                    {
                        generate_bill();
                    }
                    flag = 0;
                    break;
                }
            }
        }
        else if (uchoice == 1)
        {
            printf("Go to admin page\n");
        }
        else
        {
            printf("Invalid choice\n");
        }
    }

    close(sock);

    return 0;
}