function trsv_sym()
  condA = [];
  err_d = [];
  err_k = [];
  
  for alpha = 30:5:100
%     A = triu(rand(i));
%     alpha = i * i;
%     for j=1:i
%         A(j,j) = 1 - (1 - alpha^(-1)) * (j - 1) / (i - 1);
%     end
%     b = rand(i, 1);

    [A, b] = trsv_gen_unn(alpha, 10);
    [condA(alpha/5), err_d(alpha/5), err_k(alpha/5)] = trsv_unn_exact(10, A, b);
    alpha
  end

  err_d
  err_k
  condA

  %ax = plotyy(condA, err_d, condA, err_k);
  plot(condA, err_d, condA, err_k);
  xlabel('CondA');
  ylabel('Error');
  legend('err_d','err_k')
  %ylabel(ax(2), 'Error Kulisch');
end

function [A, b] = trsv_gen_unn(alpha, n)
    A = zeros(n,n);
    
    for i = 1:n
        A(i,i) = 1;
    end
    for i = n-1:-2:1
        A(i,n) = 1;
    end    
    for i = n-2:-2:1
        for j = i+1:n
            A(i,j) = (-1)^(i+j+1) * 2^alpha;
        end
    end
    for i = n-3:-2:1
        for j = i+1:n-1
            A(i,j) = (-1)^(i+j);
        end
    end
    
    b = zeros(n, 1);
    for i = n:-2:1
        b(i) = 1;
    end
    for i = n-1:-2:1
        b(i) = 2;
    end    
end

function x = trsv_unn_d(n, A, b)
  %x = A \ b;
  x = zeros(n, 1);
  
  %trsv for unn matrices
  for i = n:-1:1
    s = b(i);
    for j = i+1:n
      s = s - A(i,j) * x(j);
    end
    x(i) = s / A(i, i);
  end
end

function x = trsv_unn_kulisch(n, A, b)
  x = zeros(n, 1);

  %trsv for unn matrices
  for i = n:-1:1
    s = sym(b(i));
    for j = i+1:n
      s = sym(s - A(i,j) * x(j));
    end
    x(i) = double(s) / A(i, i);
  end
end

function [condA, err_d, err_k] = trsv_unn_exact(n, A, b)
  %double
  x_k = trsv_unn_kulisch(n, A, b);
  x_d = trsv_unn_d(n, A, b);

  A = sym(A);
  b = sym(b);
  x_e = sym(zeros(n, 1));

  x_e = A \ b;
  

  %compute error
  norm_x_e = max(double(abs(x_e)));
  err_k = max(double(abs(x_e - x_k))) / norm_x_e;
  err_d = max(double(abs(x_e - x_d))) / norm_x_e;

  %compute cond number
  A_inv = inv(A);
  %condA = cond(double(A), Inf);
  condA = norminf_m(A, n) * norminf_m(A_inv, n);
end

function res = norminf_m(A, n)
  res = 0.0;

  for i = 1:n
      sum = sym(0.0);
      for j = i:n
          sum = sum + abs(A(i,j));
      end
      sum_d = double(abs(sum));
      if res < sum_d
          res = sum_d;
      end
  end
end

